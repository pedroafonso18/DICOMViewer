#include "viewer/imageViewport.hpp"

#include <QPainter>

//--------------------------------------------

namespace DICOMViewer {
	namespace {

		constexpr QSize minimumViewportSize{280, 240};
		constexpr int viewportPadding = 16;
		constexpr QMargins viewportMargins{viewportPadding, viewportPadding, viewportPadding, viewportPadding};
		constexpr QColor imageBackground{17, 19, 22};
		constexpr QColor emptyStateText{183, 189, 198};

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
		update();
	}

	//--------------------------------------------

	void ImageViewport::paintEvent(QPaintEvent*)
	{
		QPainter painter(this);
		painter.fillRect(rect(), imageBackground);

		const auto area = rect().marginsRemoved(viewportMargins);
		if (currentImage.isNull()) {
			painter.setPen(emptyStateText);
			painter.drawText(area, Qt::AlignCenter, tr("Open a DICOM file to view its image."));
			return;
		}

		const QSizeF physicalSize(currentImage.width() * currentPixelAspectRatio, currentImage.height());
		const auto imageSize = physicalSize.scaled(area.size(), Qt::KeepAspectRatio);
		QRectF target(QPointF{}, imageSize);
		target.moveCenter(QRectF(area).center());
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		painter.drawImage(target, currentImage);
	}

} // namespace DICOMViewer
