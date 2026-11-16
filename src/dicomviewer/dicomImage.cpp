#include "dicomviewer/dicomImage.hpp"

#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmjpeg/djdecode.h>

#include <algorithm>
#include <fstream>
#include <utility>

//--------------------------------------------

namespace DICOMViewer {
	namespace {

		void registerJpegDecoders()
		{
			static const bool registered = [] {
				DJDecoderRegistration::registerCodecs();
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
			DcmDataset& dataset
		)
		{
			DcmElement* centers = nullptr;
			DcmElement* widths = nullptr;
			if (dataset.findAndGetElement(DCM_WindowCenter, centers).bad() ||
				dataset.findAndGetElement(DCM_WindowWidth, widths).bad()) {
				return {};
			}

			const unsigned long pairCount = std::min(centers->getVM(), widths->getVM());
			std::vector<ContrastWindow> windows;
			for (unsigned long index = 0; index < pairCount; ++index) {
				ContrastWindow window{};
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

		std::optional<Error> loadDicomFile(
			const std::filesystem::path& path,
			DcmFileFormat& file
		)
		{
			std::error_code filesystemError;
			if (!std::filesystem::is_regular_file(path, filesystemError) ||
				!std::ifstream(path, std::ios::binary)) {
				return Error{ErrorCode::FileAccess, "Cannot read the input file."};
			}

			const OFCondition loaded = file.loadFile(OFFilename(path.c_str()));
			if (loaded.bad()) {
				return Error{ErrorCode::InvalidDicom, loaded.text()};
			}
			return std::nullopt;
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
			if (metadata.photometricInterpretation != "MONOCHROME1" &&
				metadata.photometricInterpretation != "MONOCHROME2") {
				return Error{ErrorCode::UnsupportedPhotometricInterpretation,
							 metadata.photometricInterpretation};
			}

			const E_TransferSyntax originalSyntax = dataset.getOriginalXfer();
			metadata.transferSyntax = readTag(*file.getMetaInfo(), DCM_TransferSyntaxUID);
			if (metadata.transferSyntax.empty()) {
				const char* uid = DcmXfer(originalSyntax).getXferID();
				if (uid) {
					metadata.transferSyntax = uid;
				}
			}
			registerJpegDecoders();
			if (DcmXfer(originalSyntax).usesEncapsulatedFormat()) {
				const OFCondition decoded = dataset.chooseRepresentation(EXS_LittleEndianExplicit, nullptr);
				if (decoded.bad()) {
					const ErrorCode code = decoded == EC_CannotChangeRepresentation
						? ErrorCode::UnsupportedEncoding : ErrorCode::RenderFailed;
					return Error{code, decoded.text()};
				}
			}
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
			metadata.windows = readWindows(dataset);
		}

		//--------------------------------------------

		std::optional<Error> initializeRendering(
			DicomImage& image,
			Metadata& metadata,
			ContrastWindow& initialWindow
		)
		{
			if (image.getStatus() != EIS_Normal) {
				return Error{ErrorCode::RenderFailed, DicomImage::getString(image.getStatus())};
			}
			metadata.width = image.getWidth();
			metadata.height = image.getHeight();
			metadata.frameCount = image.getFrameCount();

			if (!metadata.windows.empty()) {
				initialWindow = metadata.windows.front();
			} else if (!image.setMinMaxWindow() ||
					   !image.getWindow(initialWindow.windowLevel, initialWindow.width)) {
				return Error{ErrorCode::RenderFailed, "Cannot calculate an initial window."};
			}
			return std::nullopt;
		}

	}

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

	const Metadata& Image::metadata() const noexcept
	{
		return state->metadata;
	}

	//--------------------------------------------

	ContrastWindow Image::initialWindow() const noexcept
	{
		return state->initialWindow;
	}

	//--------------------------------------------

	Result<Image> Image::open(const std::filesystem::path& path)
	{
		auto imageState = std::make_unique<ImageState>();
		DcmFileFormat& file = imageState->file;
		Metadata& metadata = imageState->metadata;
		if (std::optional<Error> error = loadDicomFile(path, file)) {
			return std::move(*error);
		}
		if (std::optional<Error> error = preparePixelData(file, metadata)) {
			return std::move(*error);
		}

		DcmDataset& dataset = *file.getDataset();
		readMetadata(dataset, metadata);
		imageState->image = std::make_unique<DicomImage>(&dataset, dataset.getCurrentXfer());
		if (std::optional<Error> error = initializeRendering(*imageState->image, metadata, imageState->initialWindow)) {
			return std::move(*error);
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
		const ContrastWindow selected = window.value_or(state->initialWindow);
		if (!selected.isValid()) {
			return Error{ErrorCode::InvalidWindow, "Window level and width must be finite, and width must be at least 1."};
		}

		DicomImage& image = *state->image;
		if (!image.setWindow(selected.windowLevel, selected.width)) {
			return Error{ErrorCode::InvalidWindow, "DCMTK rejected the requested window."};
		}

		constexpr int outputBits = 8;
		const unsigned long outputSize = image.getOutputDataSize(outputBits);
		if (outputSize == 0) {
			return Error{ErrorCode::RenderFailed, "The output frame has no pixel data."};
		}

		std::vector<std::uint8_t> pixels(outputSize);
		if (!image.getOutputData(pixels.data(), outputSize, outputBits, frame)) {
			return Error{ErrorCode::RenderFailed, "DCMTK could not render the requested frame."};
		}
		return RenderedFrame{state->metadata.width, state->metadata.height, std::move(pixels)};
	}

}
