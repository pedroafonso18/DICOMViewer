#pragma once

//--------------------------------------------

#include "dicomviewer/window.hpp"

#include <optional>
#include <vector>

//--------------------------------------------

namespace DICOMViewer {

	struct FrameContrast {
		std::optional<ContrastWindow> defaultWindow;
		std::vector<ContrastWindow> presets;
	};

} // namespace DICOMViewer
