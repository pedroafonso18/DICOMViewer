#include "dicomviewer/dicomImage.hpp"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmjpeg/djencode.h>
#include <dcmtk/dcmjpeg/djrplol.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>

//--------------------------------------------

namespace DICOMViewer {
	namespace {

		constexpr Uint16 fixtureSide = 2;
		constexpr auto pixelsPerFixtureFrame = fixtureSide * fixtureSide;
		constexpr Uint16 monochromeSamplesPerPixel = 1;
		constexpr Uint16 fixtureBitsAllocated = std::numeric_limits<Uint16>::digits;
		constexpr Uint16 fixtureBitsStored = 12;
		constexpr Uint16 unsignedPixelRepresentation = 0;
		constexpr int jpegFirstOrderPrediction = 1;
		constexpr int jpegNoPointTransform = 0;
		constexpr unsigned long firstFrameIndex = 0;
		constexpr auto displayBits = std::numeric_limits<std::uint8_t>::digits;
		constexpr Uint16 maxDisplayValue = std::numeric_limits<std::uint8_t>::max();

		template<class Frame>
		concept ExposesPixelsOnRvalue = requires(Frame&& frame) { std::move(frame).pixels(); };

		static_assert(!ExposesPixelsOnRvalue<RenderedFrame>);

		class DicomImageTest : public testing::Test {
		protected:
			void SetUp() override
			{
				std::string pattern = (std::filesystem::temp_directory_path() / "dicomviewer-XXXXXX").string();
				const char* created = mkdtemp(pattern.data());
				ASSERT_NE(created, nullptr);
				directory = created;
				path = directory / "image.dcm";
			}

			//--------------------------------------------

			void TearDown() override
			{
				if (!directory.empty()) {
					std::error_code error;
					std::filesystem::remove_all(directory, error);
				}
			}

			//--------------------------------------------

			void writeImage(
				const char* photometric,
				bool withPixels = true,
				bool compressed = false,
				const char* windowCenters = "128\\64",
				bool multiframe = false
			)
			{
				DcmFileFormat file;
				DcmDataset& dataset = *file.getDataset();
				ASSERT_TRUE(dataset.putAndInsertString(DCM_SOPClassUID,
													   UID_SecondaryCaptureImageStorage).good());
				ASSERT_TRUE(dataset.putAndInsertString(DCM_SOPInstanceUID,
													   "1.2.826.0.1.3680043.10.5432.1").good());
				ASSERT_TRUE(dataset.putAndInsertString(DCM_PhotometricInterpretation, photometric).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_SamplesPerPixel, monochromeSamplesPerPixel).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_Rows, fixtureSide).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_Columns, fixtureSide).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_BitsAllocated, fixtureBitsAllocated).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_BitsStored, fixtureBitsStored).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_HighBit, fixtureBitsStored - 1).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_PixelRepresentation, unsignedPixelRepresentation).good());
				ASSERT_TRUE(dataset.putAndInsertString(DCM_WindowCenter, windowCenters).good());
				ASSERT_TRUE(dataset.putAndInsertString(DCM_WindowWidth, "256\\128").good());
				ASSERT_TRUE(dataset.putAndInsertString(DCM_PatientOrientation, "A\\FR").good());
				if (multiframe) {
					ASSERT_TRUE(dataset.putAndInsertString(DCM_NumberOfFrames, "3").good());
					ASSERT_TRUE(dataset.putAndInsertString(DCM_SOPClassUID,
						UID_MultiframeGrayscaleWordSecondaryCaptureImageStorage).good());
				}
				if (withPixels) {
					const Uint16 pixels[] = {0, 64, 128, 255, 32, 64, 0, 255, 255, 128, 64, 0};
					ASSERT_TRUE(dataset.putAndInsertUint16Array(DCM_PixelData, pixels,
						multiframe ? std::size(pixels) : pixelsPerFixtureFrame).good());
				}

				E_TransferSyntax syntax = EXS_LittleEndianExplicit;
				if (compressed) {
					DJEncoderRegistration::registerCodecs();
					const DJ_RPLossless parameters(jpegFirstOrderPrediction, jpegNoPointTransform);
					syntax = EXS_JPEGProcess14SV1;
					const OFCondition encoded = dataset.chooseRepresentation(syntax, &parameters);
					DJEncoderRegistration::cleanup();
					ASSERT_TRUE(encoded.good()) << encoded.text();
				}
				const OFCondition saved = file.saveFile(OFFilename(path.c_str()), syntax);
				ASSERT_TRUE(saved.good()) << saved.text();
			}

			std::filesystem::path directory;
			std::filesystem::path path;
		};

		TEST_F(DicomImageTest, ClassifiesOpeningFailures)
		{
			EXPECT_EQ(std::get<Error>(Image::open(path)).code, ErrorCode::FileAccess);

			std::ofstream(path) << "This is not DICOM.";
			Result<Image> invalid = Image::open(path);
			ASSERT_TRUE(std::holds_alternative<Error>(invalid));
			EXPECT_EQ(std::get<Error>(invalid).code, ErrorCode::InvalidDicom);

			ASSERT_NO_FATAL_FAILURE(writeImage("MONOCHROME2", false));
			Result<Image> missingPixels = Image::open(path);
			ASSERT_TRUE(std::holds_alternative<Error>(missingPixels));
			EXPECT_EQ(std::get<Error>(missingPixels).code, ErrorCode::MissingPixelData);
		}

		//--------------------------------------------

		TEST_F(DicomImageTest, RendersMonochromeInStoredOrderAndKeepsPreviousBuffers)
		{
			ASSERT_NO_FATAL_FAILURE(writeImage("MONOCHROME2"));
			Result<Image> opened = Image::open(path);
			ASSERT_TRUE(std::holds_alternative<Image>(opened));
			Image& image = std::get<Image>(opened);
			EXPECT_EQ(image.metadata().orientation, "A\\FR");
			ASSERT_EQ(image.metadata().windows.size(), 2U);
			EXPECT_EQ(image.initialWindow().value().windowLevel, 128.0);
			EXPECT_EQ(image.initialWindow().value().width, 256.0);

			const Result<RenderedFrame> first = image.render();
			ASSERT_TRUE(std::holds_alternative<RenderedFrame>(first));
			const RenderedFrame& frame = std::get<RenderedFrame>(first);
			EXPECT_EQ(frame.width(), fixtureSide);
			EXPECT_EQ(frame.height(), fixtureSide);
			const std::vector<std::uint8_t> expected{0, 64, 128, 255};
			EXPECT_TRUE(std::ranges::equal(frame.pixels(), expected));

			const Result<RenderedFrame> changed = image.render(firstFrameIndex, ContrastWindow{64.0, 128.0});
			ASSERT_TRUE(std::holds_alternative<RenderedFrame>(changed));
			EXPECT_FALSE(std::ranges::equal(std::get<RenderedFrame>(changed).pixels(), expected));
			EXPECT_TRUE(std::ranges::equal(frame.pixels(), expected));
			EXPECT_EQ(std::get<Error>(image.render(1)).code, ErrorCode::FrameOutOfRange);
			EXPECT_EQ(std::get<Error>(image.render(firstFrameIndex, ContrastWindow{5.0, 0.0})).code, ErrorCode::InvalidWindow);
		}

		//--------------------------------------------

		TEST_F(DicomImageTest, KeepsValidWindowAfterUnreadablePair)
		{
			ASSERT_NO_FATAL_FAILURE(writeImage("MONOCHROME2", true, false, "invalid\\64\\32"));
			const Result<Image> opened = Image::open(path);
			ASSERT_TRUE(std::holds_alternative<Image>(opened));
			const Image& image = std::get<Image>(opened);
			ASSERT_EQ(image.metadata().windows.size(), 1U);
			EXPECT_EQ(image.initialWindow().value().windowLevel, 64.0);
			EXPECT_EQ(image.initialWindow().value().width, 128.0);
		}

		//--------------------------------------------

		TEST_F(DicomImageTest, InvertsMonochromeOnePolarity)
		{
			ASSERT_NO_FATAL_FAILURE(writeImage("MONOCHROME1"));
			Result<Image> opened = Image::open(path);
			ASSERT_TRUE(std::holds_alternative<Image>(opened));
			const Result<RenderedFrame> rendered = std::get<Image>(opened).render();
			ASSERT_TRUE(std::holds_alternative<RenderedFrame>(rendered));
			const std::vector<std::uint8_t> expected{255, 191, 127, 0};
			EXPECT_TRUE(std::ranges::equal(std::get<RenderedFrame>(rendered).pixels(), expected));
		}

		//--------------------------------------------

		TEST_F(DicomImageTest, DecodesLosslessJpeg)
		{
			ASSERT_NO_FATAL_FAILURE(writeImage("MONOCHROME2", true, true));
			Result<Image> opened = Image::open(path);
			ASSERT_TRUE(std::holds_alternative<Image>(opened));
			Image& image = std::get<Image>(opened);
			EXPECT_EQ(image.metadata().transferSyntax, UID_JPEGProcess14SV1TransferSyntax);
			const Result<RenderedFrame> rendered = image.render();
			ASSERT_TRUE(std::holds_alternative<RenderedFrame>(rendered));
			const std::vector<std::uint8_t> expected{0, 64, 128, 255};
			EXPECT_TRUE(std::ranges::equal(std::get<RenderedFrame>(rendered).pixels(), expected));
		}

		//--------------------------------------------

		TEST_F(DicomImageTest, ReadsNativeAndJpegFramesOutOfOrder)
		{
			for (const bool compressed : {false, true}) {
				SCOPED_TRACE(compressed);
				ASSERT_NO_FATAL_FAILURE(writeImage("MONOCHROME2", true, compressed, "128", true));
				auto opened = Image::open(path);
				ASSERT_TRUE(std::holds_alternative<Image>(opened));
				auto& image = std::get<Image>(opened);
				EXPECT_EQ(image.metadata().frameCount, 3U);
				const std::uint8_t expected[][pixelsPerFixtureFrame] = {{0, 64, 128, 255}, {32, 64, 0, 255}, {255, 128, 64, 0}};
				for (const unsigned long index : {2UL, 0UL, 1UL, 2UL}) {
					const auto frame = image.render(index);
					ASSERT_TRUE(std::holds_alternative<RenderedFrame>(frame));
					EXPECT_TRUE(std::ranges::equal(std::get<RenderedFrame>(frame).pixels(), expected[index]));
				}
			}
		}

		//--------------------------------------------

		TEST_F(DicomImageTest, RendersLinearExactWithFractionalWidth)
		{
			ASSERT_NO_FATAL_FAILURE(writeImage("MONOCHROME2"));
			auto opened = Image::open(path);
			ASSERT_TRUE(std::holds_alternative<Image>(opened));
			const auto frame = std::get<Image>(opened).render(firstFrameIndex, ContrastWindow{64, 0.5, VoiFunction::LinearExact});
			ASSERT_TRUE(std::holds_alternative<RenderedFrame>(frame));
			const auto pixels = std::get<RenderedFrame>(frame).pixels();
			ASSERT_EQ(pixels.size(), pixelsPerFixtureFrame);
			EXPECT_EQ(pixels[0], 0);
			EXPECT_NEAR(pixels[1], 128, 1);
			EXPECT_EQ(pixels[2], 255);
			EXPECT_EQ(pixels[3], 255);
		}

		//--------------------------------------------

		TEST_F(DicomImageTest, RendersFractionalSigmoidWidth)
		{
			ASSERT_NO_FATAL_FAILURE(writeImage("MONOCHROME2"));
			DcmFileFormat file;
			ASSERT_TRUE(file.loadFile(OFFilename(path.c_str())).good());
			auto& dataset = *file.getDataset();
			ASSERT_TRUE(dataset.putAndInsertString(DCM_VOILUTFunction, "SIGMOID").good());
			ASSERT_TRUE(dataset.putAndInsertString(DCM_WindowCenter, "64").good());
			ASSERT_TRUE(dataset.putAndInsertString(DCM_WindowWidth, "0.5").good());
			ASSERT_TRUE(file.saveFile(OFFilename(path.c_str()), EXS_LittleEndianExplicit).good());
			auto opened = Image::open(path);
			ASSERT_TRUE(std::holds_alternative<Image>(opened));
			auto& image = std::get<Image>(opened);
			ASSERT_TRUE(image.initialWindow().has_value());
			EXPECT_EQ(image.initialWindow()->width, 0.5);
			const auto rendered = image.render();
			ASSERT_TRUE(std::holds_alternative<RenderedFrame>(rendered));
			const auto pixels = std::get<RenderedFrame>(rendered).pixels();
			ASSERT_EQ(pixels.size(), pixelsPerFixtureFrame);
			EXPECT_EQ(pixels[0], 0);
			EXPECT_NEAR(pixels[1], 128, 1);
			EXPECT_EQ(pixels[2], 255);
			EXPECT_EQ(pixels[3], 255);
		}

		//--------------------------------------------

		TEST_F(DicomImageTest, RejectsIncompletePixelData)
		{
			ASSERT_NO_FATAL_FAILURE(writeImage("MONOCHROME2"));
			DcmFileFormat file;
			ASSERT_TRUE(file.loadFile(OFFilename(path.c_str())).good());
			const Uint16 pixel[] = {64};
			ASSERT_TRUE(file.getDataset()->putAndInsertUint16Array(DCM_PixelData, pixel, std::size(pixel)).good());
			ASSERT_TRUE(file.saveFile(OFFilename(path.c_str()), EXS_LittleEndianExplicit).good());
			const auto opened = Image::open(path);
			ASSERT_TRUE(std::holds_alternative<Error>(opened));
			EXPECT_EQ(std::get<Error>(opened).code, ErrorCode::IncompletePixelData);
		}


		//--------------------------------------------

		TEST_F(DicomImageTest, RestoresStoredVoiLut)
		{
			ASSERT_NO_FATAL_FAILURE(writeImage("MONOCHROME2"));
			DcmFileFormat file;
			ASSERT_TRUE(file.loadFile(OFFilename(path.c_str())).good());
			auto& dataset = *file.getDataset();
			ASSERT_TRUE(dataset.findAndDeleteElement(DCM_WindowCenter).good());
			ASSERT_TRUE(dataset.findAndDeleteElement(DCM_WindowWidth).good());
			DcmItem* lut = nullptr;
			ASSERT_TRUE(dataset.findOrCreateSequenceItem(DCM_VOILUTSequence, lut).good());
			constexpr Uint16 firstMappedLutInput = 0;
			const Uint16 values[] = {0, 32, 64, maxDisplayValue};
			const Uint16 descriptor[] = {std::size(values), firstMappedLutInput, displayBits};
			ASSERT_TRUE(lut->putAndInsertUint16Array(DcmTag(DCM_LUTDescriptor, EVR_US), descriptor, std::size(descriptor)).good());
			ASSERT_TRUE(lut->putAndInsertUint16Array(DcmTag(DCM_LUTData, EVR_US), values, std::size(values)).good());
			ASSERT_TRUE(file.saveFile(OFFilename(path.c_str()), EXS_LittleEndianExplicit).good());

			DicomImage reference(OFFilename(path.c_str()));
			ASSERT_EQ(reference.getVoiLutCount(), 1U);
			constexpr unsigned long storedLutIndex = 0;
			ASSERT_TRUE(reference.setVoiLut(storedLutIndex));
			std::vector<std::uint8_t> expected(pixelsPerFixtureFrame);
			ASSERT_TRUE(reference.getOutputData(expected.data(), expected.size(), displayBits, firstFrameIndex));
			EXPECT_EQ(expected, (std::vector<std::uint8_t>{0, 255, 255, 255}));

			auto opened = Image::open(path);
			ASSERT_TRUE(std::holds_alternative<Image>(opened));
			auto& image = std::get<Image>(opened);
			EXPECT_FALSE(image.initialWindow().has_value());
			const auto manual = image.render(firstFrameIndex, ContrastWindow{128, 256});
			ASSERT_TRUE(std::holds_alternative<RenderedFrame>(manual));
			EXPECT_FALSE(std::ranges::equal(std::get<RenderedFrame>(manual).pixels(), expected));
			const auto rendered = image.render();
			ASSERT_TRUE(std::holds_alternative<RenderedFrame>(rendered));
			EXPECT_TRUE(std::ranges::equal(std::get<RenderedFrame>(rendered).pixels(), expected));
		}

		//--------------------------------------------

		TEST(DicomAcceptanceTest, RendersReferenceMammogram)
		{
			const char* sample = std::getenv("DICOMVIEWER_REFERENCE_FILE");
			if (!sample || !*sample) {
				GTEST_SKIP() << "Set DICOMVIEWER_REFERENCE_FILE to run the reference check.";
			}
			Result<Image> opened = Image::open(sample);
			if (const Error* error = std::get_if<Error>(&opened)) {
				FAIL() << error->detail;
			}
			Image& image = std::get<Image>(opened);
			EXPECT_EQ(image.metadata().transferSyntax, UID_JPEGProcess14SV1TransferSyntax);
			EXPECT_EQ(image.metadata().orientation, "A\\FR");
			constexpr ContrastWindow referenceWindow{3091.0, 1150.0};
			EXPECT_EQ(image.initialWindow().value().windowLevel, referenceWindow.windowLevel);
			EXPECT_EQ(image.initialWindow().value().width, referenceWindow.width);

			const Result<RenderedFrame> rendered = image.render();
			if (const Error* error = std::get_if<Error>(&rendered)) {
				FAIL() << error->detail;
			}
			const RenderedFrame& frame = std::get<RenderedFrame>(rendered);
			constexpr unsigned long referenceWidth = 2394;
			constexpr unsigned long referenceHeight = 2850;
			EXPECT_EQ(frame.width(), referenceWidth);
			EXPECT_EQ(frame.height(), referenceHeight);
			ASSERT_EQ(frame.pixels().size(), referenceWidth * referenceHeight);
			const std::ranges::minmax_element_result<std::span<const std::uint8_t>::iterator> extremes =
				std::ranges::minmax_element(frame.pixels());
			EXPECT_LT(*extremes.min, *extremes.max);
		}

	} // namespace
} // namespace DICOMViewer
