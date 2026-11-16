#include "viewer/mainWindow.hpp"

#include "viewer/imageViewport.hpp"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QStatusBar>
#include <QToolBar>
#include <QtConcurrentRun>

//--------------------------------------------

namespace DICOMViewer {
	namespace {

		constexpr QSize initialWindowSize{1040, 760};
		constexpr int loadingIndicatorWidth = 92;
		constexpr int indeterminateProgress = 0;

		QString errorMessage(ErrorCode code)
		{
			switch (code) {
			case ErrorCode::FileAccess:
				return QObject::tr("The file could not be read.");
			case ErrorCode::InvalidDicom:
				return QObject::tr("The file is invalid or is not a readable DICOM.");
			case ErrorCode::MissingPixelData:
				return QObject::tr("The DICOM file does not contain pixel data.");
			case ErrorCode::IncompletePixelData:
				return QObject::tr("The DICOM file contains incomplete pixel data.");
			case ErrorCode::UnsupportedEncoding:
				return QObject::tr("This DICOM pixel encoding is not supported.");
			case ErrorCode::UnsupportedPhotometricInterpretation:
				return QObject::tr("This DICOM image's color format is not supported.");
			case ErrorCode::InvalidWindow:
			case ErrorCode::FrameOutOfRange:
			case ErrorCode::RenderFailed:
				return QObject::tr("The DICOM image could not be rendered.");
			}
			return QObject::tr("The DICOM file could not be opened.");
		}

	} // namespace

	MainWindow::MainWindow(QWidget* parent)
		: QMainWindow(parent)
	{
		setWindowTitle(tr("DICOM Viewer"));
		resize(initialWindowSize);

		viewport = new ImageViewport(this);
		setCentralWidget(viewport);

		openAction = new QAction(tr("Open…"), this);
		openAction->setShortcut(QKeySequence::Open);
		openAction->setToolTip(tr("Open a DICOM file"));
		auto* fileMenu = menuBar()->addMenu(tr("&File"));
		fileMenu->addAction(openAction);
		auto* toolbar = addToolBar(tr("File"));
		toolbar->setMovable(false);
		toolbar->addAction(openAction);

		progress = new QProgressBar(this);
		progress->setRange(indeterminateProgress, indeterminateProgress);
		progress->setTextVisible(false);
		progress->setFixedWidth(loadingIndicatorWidth);
		progress->setAccessibleName(tr("Loading DICOM"));
		statusBar()->addPermanentWidget(progress);
		progress->hide();

		connect(openAction, &QAction::triggered, this, &MainWindow::selectFile);
		connect(&watcher, &QFutureWatcherBase::finished, this, &MainWindow::finishLoading);
	}

	//--------------------------------------------

	void MainWindow::selectFile()
	{
		const auto path = QFileDialog::getOpenFileName(
			this, tr("Open DICOM"), {}, tr("DICOM files (*.dcm);;All files (*)"));
		if (path.isEmpty()) {
			return;
		}

		pendingPath = path;
		openAction->setEnabled(false);
		progress->show();
		statusBar()->showMessage(tr("Loading %1…").arg(QFileInfo(path).fileName()));
		watcher.setFuture(QtConcurrent::run([loader = loader, path] {
			return loader->load(path);
		}));
	}

	//--------------------------------------------

	void MainWindow::finishLoading()
	{
		auto result = watcher.future().takeResult();
		progress->hide();
		openAction->setEnabled(true);

		if (const auto* error = std::get_if<Error>(&result)) {
			statusBar()->showMessage(currentSummary);
			showLoadError(*error);
			return;
		}

		currentImage = std::get<LoadedImage>(std::move(result));
		const auto& metadata = currentImage->metadata;
		viewport->setImage(currentImage->image, metadata.pixelAspectRatio);
		currentSummary = tr("%1    •    %2 × %3 px")
			.arg(QFileInfo(pendingPath).fileName())
			.arg(metadata.width)
			.arg(metadata.height);
		statusBar()->showMessage(currentSummary);
	}

	//--------------------------------------------

	void MainWindow::showLoadError(const Error& error)
	{
		QMessageBox message(this);
		message.setIcon(QMessageBox::Warning);
		message.setWindowTitle(tr("Could not open DICOM"));
		message.setTextFormat(Qt::PlainText);
		message.setText(tr("Could not open %1.").arg(QFileInfo(pendingPath).fileName()));
		message.setInformativeText(errorMessage(error.code));
		if (!error.detail.empty()) {
			message.setDetailedText(QString::fromStdString(error.detail));
		}
		message.exec();
	}

} // namespace DICOMViewer
