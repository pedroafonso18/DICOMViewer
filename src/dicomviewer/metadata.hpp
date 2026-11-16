#pragma once

//--------------------------------------------

#include "dicomviewer/window.hpp"

#include <string>
#include <vector>

//--------------------------------------------

namespace DICOMViewer {

	struct Metadata {
		unsigned long width{};
		unsigned long height{};
		unsigned long frameCount{};
		std::string modality;
		std::string photometricInterpretation;
		std::string orientation;
		std::string laterality;
		std::string viewPosition;
		std::string pixelSpacing;
		std::string transferSyntax;
		unsigned int bitsStored{};
		std::vector<ContrastWindow> windows;
	};

} // namespace DICOMViewer
