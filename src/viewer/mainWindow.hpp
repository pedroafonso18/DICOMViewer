#pragma once

//--------------------------------------------

#include "viewer/imageLoader.hpp"

#include <QFutureWatcher>
#include <QMainWindow>

#include <memory>
#include <optional>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;

//--------------------------------------------

namespace DICOMViewer {

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
		void adjustContrast();
		void changeFrame(int direction);
		void restoreView();
		void startRender();
		void finishRender();
		void updateControls();
		void updateContrastControls();
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
		QDoubleSpinBox* level{};
		QDoubleSpinBox* width{};
		QPushButton* previousFrame{};
		QPushButton* nextFrame{};
		QLabel* frameIndicator{};
		std::shared_ptr<ImageLoader> loader{std::make_shared<ImageLoader>()};
		QFutureWatcher<Result<LoadedImage>> watcher;
		QFutureWatcher<Result<QImage>> renderWatcher;
		QString pendingPath;
		QString currentSummary;
		std::optional<LoadedImage> currentImage;
		FrameRequest displayedRequest;
		FrameRequest desiredRequest;
		std::optional<FrameRequest> activeRequest;
	};

} // namespace DICOMViewer
