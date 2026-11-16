#pragma once

//--------------------------------------------

#include "dicomviewer/window.hpp"

#include <optional>

//--------------------------------------------

namespace DICOMViewer {

	struct FrameRequest {
		unsigned long index{};
		std::optional<ContrastWindow> window;
		bool operator==(const FrameRequest&) const = default;
	};

} // namespace DICOMViewer
