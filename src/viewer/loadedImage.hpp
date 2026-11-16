#pragma once

//--------------------------------------------

#include "dicomviewer/metadata.hpp"

#include <QImage>

//--------------------------------------------

namespace DICOMViewer {

	struct LoadedImage {
		QImage image;
		Metadata metadata;
	};

} // namespace DICOMViewer
