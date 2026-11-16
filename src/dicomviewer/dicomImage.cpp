#include "dicomviewer/dicomImage.hpp"

#include "dicomviewer/dicomBackingFile.hpp"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcpxitem.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmdata/dccodec.h>
#include <dcmtk/dcmdata/dcrledrg.h>
#include <dcmtk/dcmimage/diregist.h>
#include <dcmtk/dcmimgle/dipixel.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#include <dcmtk/dcmjpls/djdecode.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <iterator>
#include <utility>

//--------------------------------------------

namespace DICOMViewer {
	namespace {

		constexpr Uint16 packedBitsPerPixel = 1;
		constexpr Uint16 singleSamplePerPixel = 1;
		constexpr auto bitsPerByte = std::numeric_limits<std::uint8_t>::digits;
		constexpr auto maxBitsAllocated = std::numeric_limits<Uint32>::digits;
		constexpr std::uint64_t pixelDataAlignmentBytes = 2;
		constexpr auto pixelDataAlignmentBits = pixelDataAlignmentBytes * bitsPerByte;
		constexpr unsigned long framesPerDecode = 1;
		constexpr unsigned long decodedFrameIndex = 0;
		constexpr double sigmoidTailDistance = 5.0;
		constexpr double maxLutOutput = std::numeric_limits<Uint16>::max();
		constexpr double dicomSigmoidSteepness = 4.0;
		constexpr double linearWindowCenterOffset = 0.5;
		constexpr double linearWindowWidthOffset = 1.0;

		//--------------------------------------------

		std::uint64_t paddedPixelBytes(std::uint64_t bits)
		{
			return (bits + pixelDataAlignmentBits - 1) / pixelDataAlignmentBits * pixelDataAlignmentBytes;
		}

		//--------------------------------------------

		void registerPixelDecoders()
		{
			static const bool registered = [] {
				DJDecoderRegistration::registerCodecs();
				DcmRLEDecoderRegistration::registerCodecs();
				DJLSDecoderRegistration::registerCodecs();
				return true;
			}();
			static_cast<void>(registered);
		}

		//--------------------------------------------

		std::string readTag(
			DcmItem& item,
			const DcmTagKey& tag
		)
		{
			OFString value;
			if (item.findAndGetOFStringArray(tag, value).bad()) {
				return {};
			}
			return {value.c_str(), value.size()};
		}

		//--------------------------------------------

		std::vector<ContrastWindow> readWindows(
			DcmItem& item
		)
		{
			const auto functionName = readTag(item, DCM_VOILUTFunction);
			const auto function = functionName == "SIGMOID" ? VoiFunction::Sigmoid
				: functionName == "LINEAR_EXACT" ? VoiFunction::LinearExact : VoiFunction::Linear;
			DcmElement* centers = nullptr;
			DcmElement* widths = nullptr;
			if (item.findAndGetElement(DCM_WindowCenter, centers).bad() ||
				item.findAndGetElement(DCM_WindowWidth, widths).bad()) {
				return {};
			}

			const unsigned long pairCount = std::min(centers->getVM(), widths->getVM());
			std::vector<ContrastWindow> windows;
			for (unsigned long index = 0; index < pairCount; ++index) {
				ContrastWindow window{0.0, 0.0, function};
				if (centers->getFloat64(window.windowLevel, index).bad() ||
					widths->getFloat64(window.width, index).bad()) {
					continue;
				}
				if (window.isValid()) {
					windows.push_back(window);
				}
			}
			return windows;
		}

		//--------------------------------------------

		std::vector<ContrastWindow> readFrameWindows(DcmDataset& dataset, unsigned long frame)
		{
			DcmItem* group = nullptr;
			DcmItem* voi = nullptr;
			if (dataset.findAndGetSequenceItem(DCM_PerFrameFunctionalGroupsSequence, group, frame).good() &&
				group->findAndGetSequenceItem(DCM_FrameVOILUTSequence, voi).good()) {
				auto windows = readWindows(*voi);
				if (!windows.empty()) {
					return windows;
				}
			}
			if (dataset.findAndGetSequenceItem(DCM_SharedFunctionalGroupsSequence, group).good() &&
				group->findAndGetSequenceItem(DCM_FrameVOILUTSequence, voi).good()) {
				return readWindows(*voi);
			}
			return {};
		}

		//--------------------------------------------

		bool requiresSequentialDecode(DcmDataset& dataset, unsigned long frameCount)
		{
			if (!DcmXfer(dataset.getOriginalXfer()).usesEncapsulatedFormat()) {
				return false;
			}
			DcmElement* element = nullptr;
			if (dataset.findAndGetElement(DCM_PixelData, element).bad()) {
				return false;
			}
			auto* pixels = dynamic_cast<DcmPixelData*>(element);
			if (!pixels) {
				return false;
			}
			E_TransferSyntax syntax{};
			const DcmRepresentationParameter* parameters = nullptr;
			pixels->getOriginalRepresentationKey(syntax, parameters);
			DcmPixelSequence* sequence = nullptr;
			DcmPixelItem* offsets = nullptr;
			constexpr unsigned long offsetTableItems = 1;
			return pixels->getEncapsulatedRepresentation(syntax, parameters, sequence).good() &&
				sequence->card() > frameCount + offsetTableItems &&
				sequence->getItem(offsets, 0).good() && offsets->getLengthField() == 0;
		}

		//--------------------------------------------

		std::optional<Error> preparePixelData(
			DcmFileFormat& file,
			Metadata& metadata
		)
		{
			DcmDataset& dataset = *file.getDataset();
			if (!dataset.tagExistsWithValue(DCM_PixelData)) {
				return Error{ErrorCode::MissingPixelData, "Pixel Data is absent or empty."};
			}
			metadata.photometricInterpretation = readTag(dataset, DCM_PhotometricInterpretation);
			if (metadata.photometricInterpretation.empty()) {
				return Error{ErrorCode::InvalidDicom, "Photometric Interpretation is missing."};
			}
			const bool isYbr422 = metadata.photometricInterpretation == "YBR_FULL_422" ||
				metadata.photometricInterpretation == "YBR_PARTIAL_422";
			const bool hasThreeColorSamples = metadata.photometricInterpretation == "RGB" ||
				metadata.photometricInterpretation == "YBR_FULL" || isYbr422;
			if (hasThreeColorSamples || metadata.photometricInterpretation == "PALETTE COLOR") {
				metadata.pixelFormat = PixelFormat::Rgb8;
			} else if (metadata.photometricInterpretation != "MONOCHROME1" &&
				metadata.photometricInterpretation != "MONOCHROME2") {
				return Error{ErrorCode::UnsupportedPhotometricInterpretation,
							 metadata.photometricInterpretation};
			}

			const E_TransferSyntax originalSyntax = dataset.getOriginalXfer();
			const DcmXfer syntax(originalSyntax);
			if (const char* name = syntax.getXferName()) {
				metadata.transferSyntaxName = name;
			}
			metadata.transferSyntax = readTag(*file.getMetaInfo(), DCM_TransferSyntaxUID);
			if (metadata.transferSyntax.empty()) {
				const char* uid = syntax.getXferID();
				if (uid) {
					metadata.transferSyntax = uid;
				}
			}
			registerPixelDecoders();
			if (syntax.usesEncapsulatedFormat() &&
				!DcmCodecList::canChangeCoding(originalSyntax, EXS_LittleEndianExplicit)) {
				return Error{ErrorCode::UnsupportedEncoding, metadata.transferSyntax};
			}

			Uint16 rows{}, columns{}, bitsAllocated{}, samples{};
			const auto expectedSamples = hasThreeColorSamples
				? channelCount(PixelFormat::Rgb8) : singleSamplePerPixel;
			if (dataset.findAndGetUint16(DCM_Rows, rows).bad() || rows == 0 ||
				dataset.findAndGetUint16(DCM_Columns, columns).bad() || columns == 0 ||
				dataset.findAndGetUint16(DCM_SamplesPerPixel, samples).bad() || samples != expectedSamples ||
				dataset.findAndGetUint16(DCM_BitsAllocated, bitsAllocated).bad() ||
				(bitsAllocated != packedBitsPerPixel &&
					(bitsAllocated == 0 || bitsAllocated > maxBitsAllocated || bitsAllocated % bitsPerByte != 0))) {
				return Error{ErrorCode::InvalidDicom, "Invalid pixel layout."};
			}
			constexpr Sint32 minimumFrameCount = 1;
			Sint32 frames = minimumFrameCount;
			if (dataset.tagExists(DCM_NumberOfFrames) &&
				(dataset.findAndGetSint32(DCM_NumberOfFrames, frames).bad() || frames < minimumFrameCount)) {
				return Error{ErrorCode::InvalidDicom, "Invalid Number of Frames."};
			}
			if (hasThreeColorSamples) {
				constexpr Uint16 colorByPlane = 1;
				constexpr Uint16 colorByPixel = 0;
				Uint16 planarConfiguration{};
				if (bitsAllocated == packedBitsPerPixel ||
					dataset.findAndGetUint16(DCM_PlanarConfiguration, planarConfiguration).bad() ||
					planarConfiguration > colorByPlane ||
					(isYbr422 && planarConfiguration != colorByPixel)) {
					return Error{ErrorCode::InvalidDicom, "Invalid color pixel layout."};
				}
			}
			constexpr Uint16 pixelsPerYbr422Group = 2;
			constexpr Uint16 storedSamplesPerYbr422Pixel = 2;
			const bool isNativeYbr422 = isYbr422 && syntax.usesNativeFormat();
			if (isNativeYbr422 && columns % pixelsPerYbr422Group != 0) {
				return Error{ErrorCode::InvalidDicom, "Native YBR 4:2:2 requires pixel pairs in each row."};
			}
			const auto storedSamples = isNativeYbr422 ? storedSamplesPerYbr422Pixel : samples;
			const auto frameBits = static_cast<std::uint64_t>(rows) * columns * storedSamples * bitsAllocated;
			if (paddedPixelBytes(frameBits) > std::numeric_limits<Uint32>::max()) {
				return Error{ErrorCode::UnsupportedEncoding, "One frame exceeds DCMTK's pixel buffer size."};
			}

			if (syntax.usesNativeFormat()) {
				DcmElement* pixels = nullptr;
				if (dataset.findAndGetElement(DCM_PixelData, pixels).bad()) {
					return Error{ErrorCode::MissingPixelData, "Pixel Data could not be read."};
				}
				const std::uint64_t storedBytes = pixels->getLengthField();
				const auto frameCount = static_cast<std::uint64_t>(frames);
				if (frameCount > storedBytes * bitsPerByte / frameBits) {
					return Error{ErrorCode::IncompletePixelData, "Pixel Data is shorter than the image dimensions require."};
				}
				const auto paddedBytes = paddedPixelBytes(frameBits * frameCount);
				if (storedBytes != paddedBytes) {
					return Error{ErrorCode::InvalidDicom, "Pixel Data length does not match the image dimensions."};
				}
			}
			metadata.frameCount = static_cast<unsigned long>(frames);
			return std::nullopt;
		}

		//--------------------------------------------

		void readMetadata(
			DcmDataset& dataset,
			Metadata& metadata
		)
		{
			metadata.modality = readTag(dataset, DCM_Modality);
			metadata.orientation = readTag(dataset, DCM_PatientOrientation);
			metadata.imageOrientation = readTag(dataset, DCM_ImageOrientationPatient);
			metadata.laterality = readTag(dataset, DCM_ImageLaterality);
			if (metadata.laterality.empty()) {
				metadata.laterality = readTag(dataset, DCM_Laterality);
			}
			metadata.viewPosition = readTag(dataset, DCM_ViewPosition);
			metadata.pixelSpacing = readTag(dataset, DCM_PixelSpacing);
			if (metadata.pixelSpacing.empty()) {
				metadata.pixelSpacing = readTag(dataset, DCM_ImagerPixelSpacing);
			}
			Uint16 bitsStored{};
			if (dataset.findAndGetUint16(DCM_BitsStored, bitsStored).good()) {
				metadata.bitsStored = bitsStored;
			}
			if (metadata.pixelFormat == PixelFormat::Rgb8) {
				return;
			}
			metadata.windows = readWindows(dataset);
		}

		//--------------------------------------------

		std::unique_ptr<DicomImage> decodeFrame(DcmDataset& dataset, unsigned long frame)
		{
			return std::make_unique<DicomImage>(
				&dataset, dataset.getOriginalXfer(), CIF_UsePartialAccessToPixelData, frame, framesPerDecode);
		}

		//--------------------------------------------

		bool applyFractionalSigmoid(
			DicomImage& image, const ContrastWindow& window, DcmUnsignedShort& data
		)
		{
			const auto* pixels = image.getInterData();
			if (!pixels) {
				return false;
			}
			const auto representation = pixels->getRepresentation();
			const bool signedPixels = representation == EPR_Sint8 ||
				representation == EPR_Sint16 || representation == EPR_Sint32;

			// For widths below one, values five units from the center are
			// already at the endpoints at 16-bit LUT precision.
			double minimum{}, maximum{};
			if (!image.getMinMaxValues(minimum, maximum)) {
				return false;
			}
			const double first = std::clamp(std::floor(window.windowLevel) - sigmoidTailDistance, minimum, maximum);
			const double last = std::clamp(std::ceil(window.windowLevel) + sigmoidTailDistance, minimum, maximum);
			const double firstMapped = first == last ? 0.0 : first;
			// The LUT descriptor stores its first mapped input as a signed or unsigned 16-bit value.
			const double minLutInput = signedPixels
				? std::numeric_limits<Sint16>::min() : std::numeric_limits<Uint16>::min();
			const double maxLutInput = signedPixels
				? std::numeric_limits<Sint16>::max() : std::numeric_limits<Uint16>::max();
			if (firstMapped < minLutInput || firstMapped > maxLutInput) {
				return false;
			}
			const auto count = static_cast<unsigned int>(last - first + 1);
			std::vector<Uint16> values(count);
			for (unsigned int index = 0; index < count; ++index) {
				const double exponent = -dicomSigmoidSteepness * (first + index - window.windowLevel) / window.width;
				values[index] = static_cast<Uint16>(std::lround(maxLutOutput / (1.0 + std::exp(exponent))));
			}
			const Uint16 entries[]{static_cast<Uint16>(count),
				static_cast<Uint16>(static_cast<int>(firstMapped)), std::numeric_limits<Uint16>::digits};
			DcmUnsignedShort descriptor(DcmTag(DCM_LUTDescriptor, EVR_US));
			image.setNoVoiTransformation();
			return descriptor.putUint16Array(entries, std::size(entries)).good() &&
				data.putUint16Array(values.data(), count).good() && image.setVoiLut(data, descriptor);
		}

		//--------------------------------------------

		bool applyWindow(
			DicomImage& image, const ContrastWindow& window, DcmUnsignedShort& fractionalLut
		)
		{
			if (window.function == VoiFunction::Sigmoid && window.width < ContrastWindow::minimumLinearWidth) {
				return applyFractionalSigmoid(image, window, fractionalLut);
			}
			image.setVoiLutFunction(window.function == VoiFunction::Sigmoid ? EFV_Sigmoid : EFV_Linear);
			if (window.function == VoiFunction::LinearExact) {
				// LINEAR uses center - 0.5 and width - 1 in its interpolation.
				if (window.width <= ContrastWindow::minimumLinearWidth) {
					// DCMTK's intermediate pixels are integral. At most one input
					// value is inside this window. Use a unit slope through the
					// correct output of that value, keeping its neighbors clipped.
					const double pivot = std::round(window.windowLevel);
					const double value = std::clamp(
						(pivot - window.windowLevel) / window.width + linearWindowCenterOffset, 0.0, 1.0);
					constexpr double unitSlopeLinearWidth = linearWindowWidthOffset + 1.0;
					return image.setWindow(pivot + (1.0 - value), unitSlopeLinearWidth);
				}
				return image.setWindow(window.windowLevel + linearWindowCenterOffset,
					window.width + linearWindowWidthOffset);
			}
			return image.setWindow(window.windowLevel, window.width);
		}

	}

	struct Image::ImageState {
		DicomBackingFile backing;
		DcmFileFormat file;
		// DCMTK borrows the LUT data for the lifetime of the active transform.
		DcmUnsignedShort fractionalLut{DcmTag(DCM_LUTData, EVR_US)};
		std::unique_ptr<DicomImage> image;
		Metadata metadata;
		std::optional<ContrastWindow> initialWindow;
		std::optional<unsigned long> initialLut;
		unsigned long decodedFrame{};
		bool sequentialDecode{};

		[[nodiscard]] std::optional<Error> selectFrame(unsigned long frame);
	};

	//--------------------------------------------

	std::optional<Error> Image::ImageState::selectFrame(unsigned long frame)
	{
		if (image && frame == decodedFrame) {
			return std::nullopt;
		}

		auto& dataset = *file.getDataset();
		if (sequentialDecode) {
			if (!image || frame < decodedFrame) {
				image = decodeFrame(dataset, 0);
				decodedFrame = 0;
			}
			while (image->getStatus() == EIS_Normal && decodedFrame < frame) {
				if (!image->processNextFrames(framesPerDecode) || image->getFrameCount() != framesPerDecode) {
					break;
				}
				++decodedFrame;
			}
			if (image->getStatus() != EIS_Normal || decodedFrame != frame) {
				image.reset();
				return Error{ErrorCode::RenderFailed, "Could not advance through the compressed frames."};
			}
		} else {
			auto next = decodeFrame(dataset, frame);
			if (next->getStatus() != EIS_Normal) {
				return Error{ErrorCode::RenderFailed, DicomImage::getString(next->getStatus())};
			}
			image = std::move(next);
			decodedFrame = frame;
		}
		return std::nullopt;
	}

	//--------------------------------------------

	Image::Image(std::unique_ptr<ImageState> imageState) noexcept
		: state(std::move(imageState))
	{
	}

	//--------------------------------------------

	Image::~Image() = default;

	//--------------------------------------------

	Image::Image(Image&&) noexcept = default;

	//--------------------------------------------

	Image& Image::operator=(Image&&) noexcept = default;

	//--------------------------------------------

	const Metadata& Image::metadata() const & noexcept
	{
		return state->metadata;
	}

	//--------------------------------------------

	FrameContrast Image::contrastForFrame(unsigned long frame) const
	{
		if (state->metadata.pixelFormat == PixelFormat::Rgb8 || frame >= state->metadata.frameCount) {
			return {};
		}
		auto presets = readFrameWindows(*state->file.getDataset(), frame);
		if (!presets.empty()) {
			const auto first = presets.front();
			return {first, std::move(presets)};
		}
		return {state->initialWindow, state->metadata.windows};
	}

	//--------------------------------------------

	std::optional<ContrastWindow> Image::initialWindow() const
	{
		return contrastForFrame(0).defaultWindow;
	}

	//--------------------------------------------

	Result<Image> Image::open(const std::filesystem::path& path)
	{
		auto imageState = std::make_unique<ImageState>();
		DcmFileFormat& file = imageState->file;
		Metadata& metadata = imageState->metadata;
		if (auto error = imageState->backing.open(path)) {
			return std::move(*error);
		}
		const auto loaded = file.loadFile(OFFilename(imageState->backing.readPath().c_str()));
		if (loaded.bad()) {
			return Error{ErrorCode::InvalidDicom, loaded.text()};
		}
		if (std::optional<Error> error = preparePixelData(file, metadata)) {
			return std::move(*error);
		}

		DcmDataset& dataset = *file.getDataset();
		readMetadata(dataset, metadata);
		imageState->sequentialDecode = requiresSequentialDecode(dataset, metadata.frameCount);
		imageState->image = decodeFrame(dataset, 0);
		auto& image = *imageState->image;
		if (image.getStatus() != EIS_Normal) {
			return Error{ErrorCode::RenderFailed, DicomImage::getString(image.getStatus())};
		}
		if (auto error = imageState->backing.checkUnchanged()) {
			return std::move(*error);
		}
		metadata.width = image.getWidth();
		metadata.height = image.getHeight();
		const auto ratio = image.getWidthHeightRatio();
		if (std::isfinite(ratio) && ratio > 0.0) {
			metadata.pixelAspectRatio = ratio;
		}

		if (metadata.pixelFormat == PixelFormat::Rgb8) {
			return Image{std::move(imageState)};
		}

		if (!metadata.windows.empty()) {
			imageState->initialWindow = metadata.windows.front();
		} else {
			DcmSequenceOfItems* luts = nullptr;
			if (dataset.findAndGetSequence(DCM_VOILUTSequence, luts).good()) {
				for (unsigned long index = 0; index < luts->card(); ++index) {
					if (image.setVoiLut(index)) {
						imageState->initialLut = index;
						break;
					}
				}
			}
			if (!imageState->initialLut) {
				ContrastWindow automatic{};
				if (!image.setMinMaxWindow() ||
					!image.getWindow(automatic.windowLevel, automatic.width)) {
					return Error{ErrorCode::RenderFailed, "Cannot calculate an initial window."};
				}
				imageState->initialWindow = automatic;
			}
		}
		return Image{std::move(imageState)};
	}

	//--------------------------------------------

	Result<RenderedFrame> Image::render(
		unsigned long frame,
		std::optional<ContrastWindow> window
	)
	{
		if (frame >= state->metadata.frameCount) {
			return Error{ErrorCode::FrameOutOfRange, "Frame index is outside the image."};
		}
		if (window && state->metadata.pixelFormat == PixelFormat::Rgb8) {
			return Error{ErrorCode::InvalidWindow, "Window Level and Width apply to monochrome images only."};
		}
		if (auto error = state->backing.checkUnchanged()) {
			return std::move(*error);
		}
		const auto selected = window ? window : contrastForFrame(frame).defaultWindow;
		if (selected && !selected->isValid()) {
			return Error{ErrorCode::InvalidWindow, "Window level or width is invalid for this VOI function."};
		}
		if (auto error = state->selectFrame(frame)) {
			return std::move(*error);
		}

		DicomImage& image = *state->image;
		if (selected && !applyWindow(image, *selected, state->fractionalLut)) {
			return Error{ErrorCode::InvalidWindow, "DCMTK could not apply the requested VOI window."};
		}
		if (!selected && state->initialLut && !image.setVoiLut(*state->initialLut)) {
			return Error{ErrorCode::RenderFailed, "DCMTK could not apply the stored VOI LUT."};
		}

		constexpr int outputBits = std::numeric_limits<std::uint8_t>::digits;
		const unsigned long outputSize = image.getOutputDataSize(outputBits);
		if (outputSize == 0) {
			return Error{ErrorCode::RenderFailed, "The output frame has no pixel data."};
		}

		std::vector<std::uint8_t> pixels(outputSize);
		if (!image.getOutputData(pixels.data(), outputSize, outputBits, decodedFrameIndex)) {
			return Error{ErrorCode::RenderFailed, "DCMTK could not render the requested frame."};
		}
		if (auto error = state->backing.checkUnchanged()) {
			return std::move(*error);
		}
		return RenderedFrame{state->metadata.width, state->metadata.height, state->metadata.pixelFormat, std::move(pixels)};
	}

}
