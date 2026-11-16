#include "viewer/mainWindow.hpp"

#include "viewer/contrastInput.hpp"
#include "viewer/imageViewport.hpp"
#include "viewer/metadataPanel.hpp"

#include <QAction>
#include <QComboBox>
#include <QLocale>
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
		constexpr int contrastControlsSpacing = 8;
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
		level = new ContrastInput(inputs);
		level->setObjectName(QStringLiteral("windowLevel"));
		width = new ContrastInput(inputs);
		width->setObjectName(QStringLiteral("windowWidth"));
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
		connect(level, &QLineEdit::editingFinished, this, [this] { adjustContrast(level); });
		connect(width, &QLineEdit::editingFinished, this, [this] { adjustContrast(width); });
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
		viewport->setImage(currentImage->frame.image, metadata.pixelAspectRatio);
		viewport->restoreView();
		metadataPanel->setMetadata(metadata, currentImage->frame.contrast.presets);
		desiredRequest = displayedRequest = {};
		updatePresets();
		updateContrastControls();
		updateControls();

		currentSummary = tr("%1    •    %2 × %3 px").arg(fileName).arg(metadata.width).arg(metadata.height);
		statusBar()->showMessage(currentSummary);
	}

	//--------------------------------------------

	void MainWindow::updatePresets()
	{
		const QSignalBlocker blockPresets(presets);
		const auto& contrast = currentImage->frame.contrast;
		presets->clear();
		presets->addItem(contrast.defaultWindow || currentImage->metadata.pixelFormat == PixelFormat::Rgb8
			? tr("File default") : tr("File default (VOI LUT)"));
		for (const auto& window : contrast.presets) {
			const auto inputLocale = locale();
			presets->addItem(tr("Level %1 / Width %2")
				.arg(inputLocale.toString(window.windowLevel, 'g', QLocale::FloatingPointShortest))
				.arg(inputLocale.toString(window.width, 'g', QLocale::FloatingPointShortest)));
		}
		presets->addItem(tr("Custom"));
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
			const auto& stored = currentImage->frame.contrast.presets;
			if (storedIndex < stored.size()) {
				desiredRequest.window = stored[storedIndex];
			} else {
				desiredRequest.window = desiredRequest.window.value_or(
					currentImage->frame.contrast.defaultWindow.value_or(defaultManualWindow));
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

	void MainWindow::adjustContrast(ContrastInput* editedInput)
	{
		if (!currentImage || !editedInput->isEnabled()) {
			return;
		}
		const auto previous = desiredRequest.window.value_or(
			currentImage->frame.contrast.defaultWindow.value_or(defaultManualWindow));
		auto edited = previous;
		const auto value = editedInput->number();
		if (value) {
			auto& field = editedInput == level ? edited.windowLevel : edited.width;
			field = *value;
		}
		if (!value || !edited.isValid() || edited == previous) {
			updateContrastControls();
			return;
		}
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
		auto next = std::get<RenderedImage>(std::move(result));
		const bool presetsChanged = currentImage->frame.contrast.presets != next.contrast.presets;
		currentImage->frame = std::move(next);
		if (presetsChanged) {
			updatePresets();
			metadataPanel->setMetadata(currentImage->metadata, currentImage->frame.contrast.presets);
		}
		updateContrastControls(true);
		viewport->setImage(currentImage->frame.image, currentImage->metadata.pixelAspectRatio);
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
		const bool hasActiveLut = canAdjustContrast && !currentImage->frame.contrast.defaultWindow && !desiredRequest.window;
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

	void MainWindow::updateContrastControls(bool preserveDraft)
	{
		const QSignalBlocker blockLevel(level);
		const QSignalBlocker blockWidth(width);
		const QSignalBlocker blockPresets(presets);
		const auto selected = desiredRequest.window.value_or(currentImage->frame.contrast.defaultWindow.value_or(defaultManualWindow));
		const bool hasWindow = currentImage->metadata.pixelFormat == PixelFormat::Grayscale8 &&
			(currentImage->frame.contrast.defaultWindow || desiredRequest.window);
		if (!preserveDraft || !level->hasFocus() || !level->isModified()) {
			level->showNumber(hasWindow ? std::optional{selected.windowLevel} : std::nullopt);
		}
		if (!preserveDraft || !width->hasFocus() || !width->isModified()) {
			width->showNumber(hasWindow ? std::optional{selected.width} : std::nullopt);
		}

		int index = fileDefaultPresetIndex;
		if (desiredRequest.window) {
			const auto& stored = currentImage->frame.contrast.presets;
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
