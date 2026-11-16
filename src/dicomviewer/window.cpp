#include "dicomviewer/window.hpp"

#include <cmath>

//--------------------------------------------

namespace DICOMViewer {

	bool ContrastWindow::isValid() const noexcept
	{
		return std::isfinite(windowLevel) && std::isfinite(width) &&
			(function == VoiFunction::Linear ? width >= minimumLinearWidth : width > 0.0);
	}

} // namespace DICOMViewer
