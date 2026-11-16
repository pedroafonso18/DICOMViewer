#include "viewer/mainWindow.hpp"

#include "viewer/contrastSpinBox.hpp"
#include "viewer/imageViewport.hpp"
#include "viewer/metadataPanel.hpp"

#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>

//--------------------------------------------

namespace DICOMViewer {
	namespace {

		constexpr QSize initialWindowSize{1240, 820};
		constexpr int loadingIndicatorWidth = 92;
		constexpr int indeterminateProgress = 0;
		constexpr int metadataPanelWidth = 290;
		constexpr int frameControlsPadding = 8;
		constexpr int contrastDecimals = 8;
		constexpr int contrastControlsSpacing = 8;
		constexpr double contrastInputLimit = 1e12;
		constexpr double smallestWidthInput = 0.00000001;
		constexpr ContrastWindow defaultManualWindow{0.0, ContrastWindow::minimumLinearWidth};
		constexpr int fileDefaultPresetIndex = 0;
		constexpr int firstStoredPresetIndex = 1;
		constexpr int previousFrameDirection = -1;
		constexpr int nextFrameDirection = 1;

		//--------------------------------------------

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
				return QObject::tr("The requested contrast window could not be applied.");
			case ErrorCode::FrameOutOfRange:
			case ErrorCode::RenderFailed:
				return QObject::tr("The DICOM image could not be rendered.");
			}
			return QObject::tr("The operation failed.");
		}

	} // namespace

	//--------------------------------------------

	MainWindow::MainWindow(QWidget* parent)
		: QMainWindow(parent)
	{
		setWindowTitle(tr("DICOM Viewer"));
		resize(initialWindowSize);
		createViewport();

		metadataPanel = new MetadataPanel(this);
		addDockWidget(Qt::RightDockWidgetArea, metadataPanel);
		resizeDocks({metadataPanel}, {metadataPanelWidth}, Qt::Horizontal);
		createToolbar();
		createContrastControls();

		progress = new QProgressBar(this);
		progress->setRange(indeterminateProgress, indeterminateProgress);
		progress->setTextVisible(false);
		progress->setFixedWidth(loadingIndicatorWidth);
		progress->setAccessibleName(tr("Loading DICOM"));
		statusBar()->addPermanentWidget(progress);
		progress->hide();

		connect(&watcher, &QFutureWatcherBase::finished, this, &MainWindow::finishLoading);
		connect(&renderWatcher, &QFutureWatcherBase::finished, this, &MainWindow::finishRender);
		updateControls();
	}

	//--------------------------------------------

	void MainWindow::createToolbar()
	{
		openAction = new QAction(tr("Open…"), this);
		openAction->setShortcut(QKeySequence::Open);
		openAction->setToolTip(tr("Open a DICOM file"));
		connect(openAction, &QAction::triggered, this, &MainWindow::selectFile);

		auto* toolbar = addToolBar(tr("Image tools"));
		toolbar->setMovable(false);
		toolbar->addAction(openAction);
		toolbar->addSeparator();
		fitAction = toolbar->addAction(tr("Fit to Window"), viewport, &ImageViewport::fitToWindow);
		rotateLeftAction = toolbar->addAction(tr("Rotate Left"), viewport, &ImageViewport::rotateLeft);
		rotateRightAction = toolbar->addAction(tr("Rotate Right"), viewport, &ImageViewport::rotateRight);
		restoreAction = toolbar->addAction(tr("Restore View"), this, &MainWindow::restoreView);
		fitAction->setObjectName(QStringLiteral("fitToWindow"));
		rotateRightAction->setObjectName(QStringLiteral("rotateRight"));
		restoreAction->setObjectName(QStringLiteral("restoreView"));

		auto* toggleMetadata = metadataPanel->toggleViewAction();
		toolbar->addSeparator();
		toolbar->addAction(toggleMetadata);
	}

	//--------------------------------------------

	void MainWindow::createContrastControls()
	{
		addToolBarBreak();
		auto* toolbar = addToolBar(tr("Contrast"));
		toolbar->setMovable(false);
		presets = new QComboBox(toolbar);
		presets->setObjectName(QStringLiteral("contrastPresets"));
		presets->setAccessibleName(tr("Contrast preset"));
		presets->addItem(tr("File default"));
		auto* presetLabel = new QLabel(tr("Preset"), toolbar);
		presetLabel->setBuddy(presets);
		toolbar->addWidget(presetLabel);
		toolbar->addWidget(presets);
		toolbar->addSeparator();

		auto* inputs = new QWidget(toolbar);
		auto* inputLayout = new QHBoxLayout(inputs);
		inputLayout->setContentsMargins({});
		inputLayout->setSpacing(contrastControlsSpacing);
		level = new ContrastSpinBox(inputs);
		level->setObjectName(QStringLiteral("windowLevel"));
		width = new ContrastSpinBox(inputs);
		width->setObjectName(QStringLiteral("windowWidth"));
		for (auto* input : {level, width}) {
			input->setDecimals(contrastDecimals);
			input->setKeyboardTracking(false);
			input->setMaximum(contrastInputLimit);
		}
		level->setMinimum(-contrastInputLimit);
		width->setMinimum(smallestWidthInput);
		auto* levelLabel = new QLabel(tr("Window Level"), inputs);
		levelLabel->setBuddy(level);
		inputLayout->addWidget(levelLabel);
		inputLayout->addWidget(level);
		auto* widthLabel = new QLabel(tr("Window Width"), inputs);
		widthLabel->setBuddy(width);
		inputLayout->addWidget(widthLabel);
		inputLayout->addWidget(width);
		toolbar->addWidget(inputs);

		connect(presets, &QComboBox::activated, this, &MainWindow::selectPreset);
		connect(level, &QDoubleSpinBox::valueChanged, this, &MainWindow::adjustContrast);
		connect(width, &QDoubleSpinBox::valueChanged, this, &MainWindow::adjustContrast);
	}

	//--------------------------------------------

	void MainWindow::createViewport()
	{
		auto* content = new QWidget(this);
		auto* layout = new QVBoxLayout(content);
		layout->setContentsMargins({});
		layout->setSpacing(0);
		viewport = new ImageViewport(content);
		viewport->setObjectName(QStringLiteral("imageViewport"));
		layout->addWidget(viewport, 1);

		auto* frameControls = new QHBoxLayout;
		frameControls->setContentsMargins(0, frameControlsPadding, 0, frameControlsPadding);
		frameControls->setSpacing(frameControlsPadding);
		frameControls->addStretch();
		previousFrame = new QPushButton(tr("Previous"), content);
		previousFrame->setObjectName(QStringLiteral("previousFrame"));
		nextFrame = new QPushButton(tr("Next"), content);
		nextFrame->setObjectName(QStringLiteral("nextFrame"));
		frameIndicator = new QLabel(tr("No frame"), content);
		frameIndicator->setObjectName(QStringLiteral("frameIndicator"));
		frameIndicator->setAlignment(Qt::AlignCenter);
		frameControls->addWidget(previousFrame);
		frameControls->addWidget(frameIndicator);
		frameControls->addWidget(nextFrame);
		frameControls->addStretch();
		layout->addLayout(frameControls);
		setCentralWidget(content);

		connect(previousFrame, &QPushButton::clicked, this, [this] { changeFrame(previousFrameDirection); });
		connect(nextFrame, &QPushButton::clicked, this, [this] { changeFrame(nextFrameDirection); });
	}

	//--------------------------------------------

	void MainWindow::selectFile()
	{
		const auto path = QFileDialog::getOpenFileName(
			this, tr("Open DICOM"), {}, tr("DICOM files (*.dcm *.DCM);;All files (*)"));
		if (path.isEmpty()) {
			return;
		}

		pendingPath = path;
		progress->show();
		statusBar()->showMessage(tr("Loading %1…").arg(QFileInfo(path).fileName()));
		updateControls();
		watcher.setFuture(QtConcurrent::run([loader = loader, path] { return loader->load(path); }));
	}

	//--------------------------------------------

	void MainWindow::finishLoading()
	{
		auto result = watcher.future().takeResult();
		const auto fileName = QFileInfo(pendingPath).fileName();
		pendingPath.clear();
		progress->hide();

		if (const auto* error = std::get_if<Error>(&result)) {
			statusBar()->showMessage(currentSummary);
			updateControls();
			showError(tr("Could not open DICOM"), tr("Could not open %1.").arg(fileName), *error);
			return;
		}

		currentImage = std::get<LoadedImage>(std::move(result));
		const auto& metadata = currentImage->metadata;
		viewport->setImage(currentImage->image, metadata.pixelAspectRatio);
		viewport->restoreView();
		metadataPanel->setMetadata(metadata);
		desiredRequest = displayedRequest = {};
		presets->clear();
		presets->addItem(currentImage->initialWindow || metadata.pixelFormat == PixelFormat::Rgb8
			? tr("File default") : tr("File default (VOI LUT)"));
		for (const auto& window : metadata.windows) {
			presets->addItem(tr("Level %1 / Width %2").arg(window.windowLevel).arg(window.width));
		}
		presets->addItem(tr("Custom"));
		updateContrastControls();
		updateControls();

		currentSummary = tr("%1    •    %2 × %3 px").arg(fileName).arg(metadata.width).arg(metadata.height);
		statusBar()->showMessage(currentSummary);
	}

	//--------------------------------------------

	void MainWindow::selectPreset(int index)
	{
		if (!currentImage || index < fileDefaultPresetIndex) {
			return;
		}
		if (index == fileDefaultPresetIndex) {
			desiredRequest.window.reset();
		} else {
			const auto storedIndex = static_cast<std::size_t>(index - firstStoredPresetIndex);
			const auto& stored = currentImage->metadata.windows;
			if (storedIndex < stored.size()) {
				desiredRequest.window = stored[storedIndex];
			} else {
				desiredRequest.window = desiredRequest.window.value_or(
					currentImage->initialWindow.value_or(defaultManualWindow));
			}
		}
		updateContrastControls();
		startRender();
		if (index == presets->count() - 1) {
			level->setFocus();
			level->selectAll();
		}
	}

	//--------------------------------------------

	void MainWindow::adjustContrast()
	{
		if (!currentImage || currentImage->metadata.pixelFormat == PixelFormat::Rgb8) {
			return;
		}
		auto edited = desiredRequest.window.value_or(currentImage->initialWindow.value_or(defaultManualWindow));
		edited.windowLevel = level->value();
		edited.width = width->value();
		desiredRequest.window = edited;
		updateContrastControls();
		startRender();
	}

	//--------------------------------------------

	void MainWindow::changeFrame(int direction)
	{
		if (direction == previousFrameDirection && desiredRequest.index > 0) {
			--desiredRequest.index;
		} else if (direction == nextFrameDirection &&
			desiredRequest.index + 1 < currentImage->metadata.frameCount) {
			++desiredRequest.index;
		}
		startRender();
	}

	//--------------------------------------------

	void MainWindow::restoreView()
	{
		viewport->restoreView();
		desiredRequest = {};
		updateContrastControls();
		startRender();
	}

	//--------------------------------------------

	void MainWindow::startRender()
	{
		if (activeRequest || desiredRequest == displayedRequest) {
			updateControls();
			return;
		}
		activeRequest = desiredRequest;
		updateControls();
		renderWatcher.setFuture(QtConcurrent::run([loader = loader, request = *activeRequest] {
			return loader->render(request);
		}));
	}

	//--------------------------------------------

	void MainWindow::finishRender()
	{
		auto result = renderWatcher.future().takeResult();
		const auto completed = *activeRequest;
		activeRequest.reset();

		if (completed != desiredRequest) {
			startRender();
			return;
		}
		if (const auto* error = std::get_if<Error>(&result)) {
			desiredRequest = displayedRequest;
			updateContrastControls();
			updateControls();
			showError(tr("Could not update image"), tr("The last rendered frame is still displayed."), *error);
			return;
		}
		currentImage->image = std::get<QImage>(std::move(result));
		viewport->setImage(currentImage->image, currentImage->metadata.pixelAspectRatio);
		displayedRequest = completed;
		updateControls();
	}

	//--------------------------------------------

	void MainWindow::updateControls()
	{
		const bool isLoading = !pendingPath.isEmpty();
		const bool hasImage = currentImage.has_value();
		const bool canEdit = hasImage && !isLoading;
		const bool canAdjustContrast = canEdit && currentImage->metadata.pixelFormat == PixelFormat::Grayscale8;
		openAction->setEnabled(!isLoading && !activeRequest);
		for (auto* action : {fitAction, restoreAction, rotateLeftAction, rotateRightAction}) {
			action->setEnabled(canEdit);
		}
		const bool hasActiveLut = canAdjustContrast && !currentImage->initialWindow && !desiredRequest.window;
		presets->setEnabled(canAdjustContrast);
		level->setEnabled(canAdjustContrast && !hasActiveLut);
		width->setEnabled(canAdjustContrast && !hasActiveLut);
		previousFrame->setEnabled(canEdit && desiredRequest.index > 0);
		nextFrame->setEnabled(canEdit && desiredRequest.index + 1 < currentImage->metadata.frameCount);
		if (hasImage) {
			frameIndicator->setText(tr("%1 of %2").arg(displayedRequest.index + 1).arg(currentImage->metadata.frameCount));
		}
	}

	//--------------------------------------------

	void MainWindow::updateContrastControls()
	{
		const QSignalBlocker blockLevel(level);
		const QSignalBlocker blockWidth(width);
		const QSignalBlocker blockPresets(presets);
		const auto selected = desiredRequest.window.value_or(currentImage->initialWindow.value_or(defaultManualWindow));
		width->setMinimum(selected.function == VoiFunction::Linear ? ContrastWindow::minimumLinearWidth : smallestWidthInput);
		const bool hasActiveLut = currentImage->metadata.pixelFormat == PixelFormat::Grayscale8 &&
			!currentImage->initialWindow && !desiredRequest.window;
		const auto placeholder = hasActiveLut ? QStringLiteral("—") : QString{};
		level->setSpecialValueText(placeholder);
		width->setSpecialValueText(placeholder);
		level->setValue(hasActiveLut ? level->minimum() : selected.windowLevel);
		width->setValue(hasActiveLut ? width->minimum() : selected.width);

		int index = fileDefaultPresetIndex;
		if (desiredRequest.window) {
			const auto& stored = currentImage->metadata.windows;
			const auto match = std::ranges::find(stored, selected);
			index = firstStoredPresetIndex + static_cast<int>(match - stored.begin());
		}
		presets->setCurrentIndex(index);
	}

	//--------------------------------------------

	void MainWindow::showError(const QString& title, const QString& context, const Error& error)
	{
		QMessageBox message(this);
		message.setIcon(QMessageBox::Warning);
		message.setWindowTitle(title);
		message.setTextFormat(Qt::PlainText);
		message.setText(context);
		message.setInformativeText(errorMessage(error.code));
		if (!error.detail.empty()) {
			message.setDetailedText(QString::fromStdString(error.detail));
		}
		message.exec();
	}

} // namespace DICOMViewer
