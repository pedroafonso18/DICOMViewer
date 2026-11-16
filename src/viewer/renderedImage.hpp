#pragma once

//--------------------------------------------

#include "dicomviewer/frameContrast.hpp"

#include <QImage>

//--------------------------------------------

namespace DICOMViewer {

	struct RenderedImage {
		QImage image;
		FrameContrast contrast;
	};

} // namespace DICOMViewer
