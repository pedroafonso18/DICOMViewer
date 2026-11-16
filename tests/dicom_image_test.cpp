#include "dicomviewer/dicomImage.hpp"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmjpeg/djencode.h>
#include <dcmtk/dcmjpeg/djrplol.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>

//--------------------------------------------

namespace DICOMViewer {
	namespace {

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
				const char* windowCenters = "128\\64"
			)
			{
				DcmFileFormat file;
				DcmDataset& dataset = *file.getDataset();
				ASSERT_TRUE(dataset.putAndInsertString(DCM_SOPClassUID,
													   UID_SecondaryCaptureImageStorage).good());
				ASSERT_TRUE(dataset.putAndInsertString(DCM_SOPInstanceUID,
													   "1.2.826.0.1.3680043.10.5432.1").good());
				ASSERT_TRUE(dataset.putAndInsertString(DCM_PhotometricInterpretation, photometric).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_SamplesPerPixel, 1).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_Rows, 2).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_Columns, 2).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_BitsAllocated, 16).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_BitsStored, 12).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_HighBit, 11).good());
				ASSERT_TRUE(dataset.putAndInsertUint16(DCM_PixelRepresentation, 0).good());
				ASSERT_TRUE(dataset.putAndInsertString(DCM_WindowCenter, windowCenters).good());
				ASSERT_TRUE(dataset.putAndInsertString(DCM_WindowWidth, "256\\128").good());
				ASSERT_TRUE(dataset.putAndInsertString(DCM_PatientOrientation, "A\\FR").good());
				if (withPixels) {
					const Uint16 pixels[] = {0, 64, 128, 255};
					ASSERT_TRUE(dataset.putAndInsertUint16Array(DCM_PixelData, pixels, 4).good());
				}

				E_TransferSyntax syntax = EXS_LittleEndianExplicit;
				if (compressed) {
					DJEncoderRegistration::registerCodecs();
					const DJ_RPLossless parameters(1, 0);
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
			EXPECT_EQ(image.initialWindow().windowLevel, 128.0);
			EXPECT_EQ(image.initialWindow().width, 256.0);

			const Result<RenderedFrame> first = image.render();
			ASSERT_TRUE(std::holds_alternative<RenderedFrame>(first));
			const RenderedFrame& frame = std::get<RenderedFrame>(first);
			EXPECT_EQ(frame.width(), 2U);
			EXPECT_EQ(frame.height(), 2U);
			const std::vector<std::uint8_t> expected{0, 64, 128, 255};
			EXPECT_TRUE(std::ranges::equal(frame.pixels(), expected));

			const Result<RenderedFrame> changed = image.render(0, ContrastWindow{64.0, 128.0});
			ASSERT_TRUE(std::holds_alternative<RenderedFrame>(changed));
			EXPECT_FALSE(std::ranges::equal(std::get<RenderedFrame>(changed).pixels(), expected));
			EXPECT_TRUE(std::ranges::equal(frame.pixels(), expected));
			EXPECT_EQ(std::get<Error>(image.render(1)).code, ErrorCode::FrameOutOfRange);
			EXPECT_EQ(std::get<Error>(image.render(0, ContrastWindow{5.0, 0.0})).code, ErrorCode::InvalidWindow);
		}

		//--------------------------------------------

		TEST_F(DicomImageTest, KeepsValidWindowAfterUnreadablePair)
		{
			ASSERT_NO_FATAL_FAILURE(writeImage("MONOCHROME2", true, false, "invalid\\64\\32"));
			const Result<Image> opened = Image::open(path);
			ASSERT_TRUE(std::holds_alternative<Image>(opened));
			const Image& image = std::get<Image>(opened);
			ASSERT_EQ(image.metadata().windows.size(), 1U);
			EXPECT_EQ(image.initialWindow().windowLevel, 64.0);
			EXPECT_EQ(image.initialWindow().width, 128.0);
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
			EXPECT_EQ(image.initialWindow().windowLevel, 3091.0);
			EXPECT_EQ(image.initialWindow().width, 1150.0);

			const Result<RenderedFrame> rendered = image.render();
			if (const Error* error = std::get_if<Error>(&rendered)) {
				FAIL() << error->detail;
			}
			const RenderedFrame& frame = std::get<RenderedFrame>(rendered);
			EXPECT_EQ(frame.width(), 2394U);
			EXPECT_EQ(frame.height(), 2850U);
			ASSERT_EQ(frame.pixels().size(), 2394U * 2850U);
			const std::ranges::minmax_element_result<std::span<const std::uint8_t>::iterator> extremes =
				std::ranges::minmax_element(frame.pixels());
			EXPECT_LT(*extremes.min, *extremes.max);
		}

	} // namespace
} // namespace DICOMViewer
