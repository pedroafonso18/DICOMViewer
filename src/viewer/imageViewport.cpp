#include "viewer/imageViewport.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

//--------------------------------------------

namespace DICOMViewer {
	namespace {

		constexpr QSize minimumViewportSize{280, 240};
		constexpr int viewportPadding = 16;
		constexpr QMargins viewportMargins{viewportPadding, viewportPadding, viewportPadding, viewportPadding};
		constexpr QColor imageBackground{17, 19, 22};
		constexpr QColor emptyStateText{183, 189, 198};
		constexpr int quarterTurnDegrees = 90;
		constexpr int quarterTurnsPerRevolution = 4;
		constexpr int quarterTurnsPerHalfRevolution = quarterTurnsPerRevolution / 2;
		constexpr double fitZoom = 1.0;
		constexpr double minimumZoom = 0.25;
		constexpr double maximumZoom = 16.0;
		constexpr double zoomPerWheelStep = 1.2;
		constexpr double angleUnitsPerWheelStep = 120.0;

	} // namespace

	//--------------------------------------------

	ImageViewport::ImageViewport(QWidget* parent)
		: QWidget(parent)
	{
		setMinimumSize(minimumViewportSize);
		setAccessibleName(tr("DICOM image"));
	}

	//--------------------------------------------

	void ImageViewport::setImage(const QImage& image, double pixelAspectRatio)
	{
		currentImage = image;
		currentPixelAspectRatio = pixelAspectRatio;
		setCursor(Qt::OpenHandCursor);
		update();
	}

	//--------------------------------------------

	void ImageViewport::fitToWindow()
	{
		zoom = fitZoom;
		pan = {};
		update();
	}

	//--------------------------------------------

	void ImageViewport::restoreView()
	{
		rotation = 0;
		fitToWindow();
	}

	//--------------------------------------------

	void ImageViewport::rotateLeft()
	{
		rotateBy(-1);
	}

	//--------------------------------------------

	void ImageViewport::rotateRight()
	{
		rotateBy(1);
	}

	//--------------------------------------------

	void ImageViewport::rotateBy(int quarterTurns)
	{
		rotation = (rotation + quarterTurns) % quarterTurnsPerRevolution;
		update();
	}

	//--------------------------------------------

	QRect ImageViewport::imageArea() const noexcept
	{
		return rect().marginsRemoved(viewportMargins);
	}

	//--------------------------------------------

	void ImageViewport::paintEvent(QPaintEvent*)
	{
		QPainter painter(this);
		painter.fillRect(rect(), imageBackground);

		const auto area = imageArea();
		if (currentImage.isNull()) {
			painter.setPen(emptyStateText);
			painter.drawText(area, Qt::AlignCenter, tr("Open a DICOM file to view its image."));
			return;
		}

		const bool swapsAxes = rotation % quarterTurnsPerHalfRevolution != 0;
		QSizeF fittedBounds(currentImage.width() * currentPixelAspectRatio, currentImage.height());
		if (swapsAxes) {
			fittedBounds.transpose();
		}
		fittedBounds.scale(area.size(), Qt::KeepAspectRatio);
		if (swapsAxes) {
			fittedBounds.transpose();
		}
		QRectF target(QPointF{}, fittedBounds);
		target.moveCenter(QPointF{});

		painter.setClipRect(area);
		painter.translate(QRectF(area).center() + pan);
		painter.rotate(rotation * quarterTurnDegrees);
		painter.scale(zoom, zoom);
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		painter.drawImage(target, currentImage);
	}

	//--------------------------------------------

	void ImageViewport::wheelEvent(QWheelEvent* event)
	{
		if (currentImage.isNull() || event->angleDelta().y() == 0) {
			event->ignore();
			return;
		}
		const double steps = event->angleDelta().y() / angleUnitsPerWheelStep;
		const double nextZoom = std::clamp(zoom * std::pow(zoomPerWheelStep, steps), minimumZoom, maximumZoom);
		const QPointF cursorFromCenter = event->position() - QRectF(imageArea()).center();
		pan = cursorFromCenter - (cursorFromCenter - pan) * (nextZoom / zoom);
		zoom = nextZoom;
		update();
		event->accept();
	}

	//--------------------------------------------

	void ImageViewport::mousePressEvent(QMouseEvent* event)
	{
		if (!currentImage.isNull() && event->button() == Qt::LeftButton) {
			dragging = true;
			dragPosition = event->position();
			setCursor(Qt::ClosedHandCursor);
			event->accept();
			return;
		}
		QWidget::mousePressEvent(event);
	}

	//--------------------------------------------

	void ImageViewport::mouseMoveEvent(QMouseEvent* event)
	{
		if (dragging && event->buttons().testFlag(Qt::LeftButton)) {
			pan += event->position() - dragPosition;
			dragPosition = event->position();
			update();
			event->accept();
			return;
		}
		QWidget::mouseMoveEvent(event);
	}

	//--------------------------------------------

	void ImageViewport::mouseReleaseEvent(QMouseEvent* event)
	{
		if (dragging && event->button() == Qt::LeftButton) {
			dragging = false;
			setCursor(Qt::OpenHandCursor);
			event->accept();
			return;
		}
		QWidget::mouseReleaseEvent(event);
	}

} // namespace DICOMViewer
