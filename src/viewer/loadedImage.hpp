#pragma once

//--------------------------------------------

#include "dicomviewer/metadata.hpp"

#include <QImage>

#include <optional>

//--------------------------------------------

namespace DICOMViewer {

	struct LoadedImage {
		QImage image;
		std::optional<ContrastWindow> initialWindow;
		Metadata metadata;
	};

} // namespace DICOMViewer
