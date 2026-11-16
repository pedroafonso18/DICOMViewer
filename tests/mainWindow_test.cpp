#include "viewer/mainWindow.hpp"

#include <dcmtk/dcmdata/dctk.h>
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>

#include <array>
#include <chrono>
#include <limits>

//--------------------------------------------

namespace DICOMViewer {
	namespace {

		constexpr Uint16 fixtureSide = 2;
		constexpr Uint16 monochromeSamplesPerPixel = 1;
		constexpr Uint16 bitsPerSample = std::numeric_limits<Uint8>::digits;
		constexpr Uint16 unsignedPixelRepresentation = 0;
		constexpr auto maxPixelValue = std::numeric_limits<Uint8>::max();
		constexpr auto dialogPollInterval = std::chrono::milliseconds{10};
		constexpr auto dialogDeadline = std::chrono::seconds{5};

		void writeDicom(const QString& path)
		{
			DcmFileFormat file;
			auto& dataset = *file.getDataset();
			ASSERT_TRUE(dataset.putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage).good());
			ASSERT_TRUE(dataset.putAndInsertString(DCM_SOPInstanceUID, "1.2.826.0.1.3680043.10.5432.2").good());
			ASSERT_TRUE(dataset.putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2").good());
			ASSERT_TRUE(dataset.putAndInsertString(DCM_PixelAspectRatio, "2\\1").good());
			ASSERT_TRUE(dataset.putAndInsertUint16(DCM_SamplesPerPixel, monochromeSamplesPerPixel).good());
			ASSERT_TRUE(dataset.putAndInsertUint16(DCM_Rows, fixtureSide).good());
			ASSERT_TRUE(dataset.putAndInsertUint16(DCM_Columns, fixtureSide).good());
			ASSERT_TRUE(dataset.putAndInsertUint16(DCM_BitsAllocated, bitsPerSample).good());
			ASSERT_TRUE(dataset.putAndInsertUint16(DCM_BitsStored, bitsPerSample).good());
			ASSERT_TRUE(dataset.putAndInsertUint16(DCM_HighBit, bitsPerSample - 1).good());
			ASSERT_TRUE(dataset.putAndInsertUint16(DCM_PixelRepresentation, unsignedPixelRepresentation).good());
			const std::array<Uint8, fixtureSide * fixtureSide> pixels{0, maxPixelValue, maxPixelValue, maxPixelValue};
			ASSERT_TRUE(dataset.putAndInsertUint8Array(DCM_PixelData, pixels.data(), pixels.size()).good());
			ASSERT_TRUE(file.saveFile(OFFilename(QFile::encodeName(path).constData()),
				EXS_LittleEndianExplicit).good());
		}

		//--------------------------------------------

		TEST(MainWindowTest, PreservesSuccessfulLoadAfterAnInvalidFile)
		{
			QTemporaryDir directory;
			ASSERT_TRUE(directory.isValid());
			const auto validPath = directory.filePath("valid.dcm");
			ASSERT_NO_FATAL_FAILURE(writeDicom(validPath));
			const auto invalidPath = directory.filePath("invalid.dcm");
			QFile invalid(invalidPath);
			ASSERT_TRUE(invalid.open(QIODevice::WriteOnly));
			ASSERT_GT(invalid.write("Not DICOM"), 0);
			invalid.close();

			MainWindow window;
			window.show();
			QAction* open = nullptr;
			for (auto* action : window.findChildren<QAction*>()) {
				if (action->shortcut() == QKeySequence::Open) {
					open = action;
					break;
				}
			}
			ASSERT_NE(open, nullptr);

			QEventLoop loop;
			QTimer driver;
			QTimer deadline;
			deadline.setSingleShot(true);
			QObject::connect(&deadline, &QTimer::timeout, &loop, [&] {
				for (auto* widget : QApplication::topLevelWidgets()) {
					if (auto* dialog = qobject_cast<QDialog*>(widget); dialog && dialog->isVisible()) {
						dialog->reject();
					}
				}
				loop.exit(1);
			});

			const std::array paths{validPath, invalidPath};
			constexpr std::size_t successfulLoadsBeforeError = 1;
			std::size_t requestedFiles = 0;
			std::size_t selectedFiles = 0;
			std::size_t disabledDuringLoad = 0;
			bool sawError = false;
			QString summary;
			QImage viewport;
			QObject::connect(&driver, &QTimer::timeout, &loop, [&] {
				for (auto* widget : QApplication::topLevelWidgets()) {
					if (!widget->isVisible()) {
						continue;
					}
					if (auto* picker = qobject_cast<QFileDialog*>(widget)) {
						auto* input = picker->findChild<QLineEdit*>("fileNameEdit");
						if (input && selectedFiles < requestedFiles) {
							input->setText(paths[selectedFiles++]);
							QMetaObject::invokeMethod(picker, "accept", Qt::DirectConnection);
						}
						return;
					}
					if (auto* error = qobject_cast<QMessageBox*>(widget)) {
						sawError = error->text().contains("invalid.dcm");
						error->accept();
						return;
					}
				}
				if (!open->isEnabled()) {
					return;
				}
				if (requestedFiles == paths.size()) {
					loop.quit();
					return;
				}
				if (requestedFiles == successfulLoadsBeforeError) {
					summary = window.statusBar()->currentMessage();
					viewport = window.centralWidget()->grab().toImage();
				}
				++requestedFiles;
				QTimer::singleShot(0, &loop, [&] {
					open->trigger();
					if (!open->isEnabled()) {
						++disabledDuringLoad;
					}
				});
			});
			driver.start(dialogPollInterval);
			deadline.start(dialogDeadline);
			ASSERT_EQ(loop.exec(), 0);
			EXPECT_EQ(selectedFiles, paths.size());
			EXPECT_EQ(disabledDuringLoad, paths.size());
			EXPECT_TRUE(sawError);
			EXPECT_TRUE(open->isEnabled());
			EXPECT_TRUE(summary.contains("valid.dcm"));
			EXPECT_EQ(window.statusBar()->currentMessage(), summary);
			ASSERT_FALSE(viewport.isNull());
			EXPECT_EQ(window.centralWidget()->grab().toImage(), viewport);

			const auto center = viewport.rect().center();
			const auto background = viewport.pixelColor(viewport.rect().topLeft());
			// A third of the viewport height is inside the tall image vertically
			// and outside its narrower bounds horizontally.
			constexpr int geometryProbeDivisor = 3;
			const int geometryProbeOffset = viewport.height() / geometryProbeDivisor;
			EXPECT_NE(viewport.pixelColor(center.x(), center.y() + geometryProbeOffset), background);
			EXPECT_EQ(viewport.pixelColor(center.x() + geometryProbeOffset, center.y()), background);
		}

	} // namespace
} // namespace DICOMViewer

//--------------------------------------------

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
	QApplication application(argc, argv);
	return RUN_ALL_TESTS();
}
