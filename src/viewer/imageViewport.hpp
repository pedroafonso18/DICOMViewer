#pragma once

//--------------------------------------------

#include <QImage>
#include <QWidget>

//--------------------------------------------

namespace DICOMViewer {

	class ImageViewport final : public QWidget {
	public:
		explicit ImageViewport(QWidget* parent = nullptr);

		void setImage(const QImage& image, double pixelAspectRatio);

	protected:
		void paintEvent(QPaintEvent* event) override;

	private:
		QImage currentImage;
		double currentPixelAspectRatio{1.0};
	};

} // namespace DICOMViewer
