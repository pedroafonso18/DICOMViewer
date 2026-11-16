#pragma once

//--------------------------------------------

#include "viewer/imageLoader.hpp"

#include <QFutureWatcher>
#include <QMainWindow>

#include <memory>
#include <optional>

class QAction;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;

//--------------------------------------------

namespace DICOMViewer {

	class ContrastInput;
	class ImageViewport;
	class MetadataPanel;

	class MainWindow final : public QMainWindow {
	public:
		explicit MainWindow(QWidget* parent = nullptr);

	private:
		void createToolbar();
		void createContrastControls();
		void createViewport();
		void selectFile();
		void finishLoading();
		void selectPreset(int index);
		void adjustContrast(ContrastInput* editedInput);
		void updatePresets();
		void changeFrame(int direction);
		void restoreView();
		void startRender();
		void finishRender();
		void updateControls();
		void updateContrastControls(bool preserveDraft = false);
		void showError(const QString& title, const QString& context, const Error& error);

		ImageViewport* viewport{};
		MetadataPanel* metadataPanel{};
		QAction* openAction{};
		QAction* fitAction{};
		QAction* restoreAction{};
		QAction* rotateLeftAction{};
		QAction* rotateRightAction{};
		QProgressBar* progress{};
		QComboBox* presets{};
		ContrastInput* level{};
		ContrastInput* width{};
		QPushButton* previousFrame{};
		QPushButton* nextFrame{};
		QLabel* frameIndicator{};
		std::shared_ptr<ImageLoader> loader{std::make_shared<ImageLoader>()};
		QFutureWatcher<Result<LoadedImage>> watcher;
		QFutureWatcher<Result<RenderedImage>> renderWatcher;
		QString pendingPath;
		QString currentSummary;
		std::optional<LoadedImage> currentImage;
		FrameRequest displayedRequest;
		FrameRequest desiredRequest;
		std::optional<FrameRequest> activeRequest;
	};

} // namespace DICOMViewer
