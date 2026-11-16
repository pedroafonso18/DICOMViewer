#pragma once

//--------------------------------------------

#include "dicomviewer/metadata.hpp"

#include "viewer/renderedImage.hpp"

//--------------------------------------------

namespace DICOMViewer {

	struct LoadedImage {
		RenderedImage frame;
		Metadata metadata;
	};

} // namespace DICOMViewer
