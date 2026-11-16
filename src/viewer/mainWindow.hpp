#pragma once

//--------------------------------------------

#include "viewer/imageLoader.hpp"

#include <QFutureWatcher>
#include <QMainWindow>

#include <memory>
#include <optional>

class QAction;
class QProgressBar;

//--------------------------------------------

namespace DICOMViewer {

	class ImageViewport;

	class MainWindow final : public QMainWindow {
	public:
		explicit MainWindow(QWidget* parent = nullptr);

	private:
		void selectFile();
		void finishLoading();
		void showLoadError(const Error& error);

		ImageViewport* viewport{};
		QAction* openAction{};
		QProgressBar* progress{};
		std::shared_ptr<ImageLoader> loader{std::make_shared<ImageLoader>()};
		QFutureWatcher<Result<LoadedImage>> watcher;
		QString pendingPath;
		QString currentSummary;
		std::optional<LoadedImage> currentImage;
	};

} // namespace DICOMViewer
