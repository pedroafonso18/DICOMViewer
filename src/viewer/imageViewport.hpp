#pragma once

//--------------------------------------------

#include <QImage>
#include <QPointF>
#include <QWidget>

//--------------------------------------------

namespace DICOMViewer {

	class ImageViewport final : public QWidget {
	public:
		explicit ImageViewport(QWidget* parent = nullptr);

		void setImage(const QImage& image, double pixelAspectRatio);
		void fitToWindow();
		void restoreView();
		void rotateLeft();
		void rotateRight();

	protected:
		void paintEvent(QPaintEvent* event) override;
		void wheelEvent(QWheelEvent* event) override;
		void mousePressEvent(QMouseEvent* event) override;
		void mouseMoveEvent(QMouseEvent* event) override;
		void mouseReleaseEvent(QMouseEvent* event) override;

	private:
		[[nodiscard]] QRect imageArea() const noexcept;
		void rotateBy(int quarterTurns);

		QImage currentImage;
		QPointF pan;
		QPointF dragPosition;
		double zoom{1.0};
		int rotation{};
		bool dragging{};
		double currentPixelAspectRatio{1.0};
	};

} // namespace DICOMViewer
