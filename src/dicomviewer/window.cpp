#include "dicomviewer/window.hpp"

#include <cmath>

//--------------------------------------------

namespace DICOMViewer {

	bool ContrastWindow::isValid() const noexcept
	{
		return std::isfinite(windowLevel) && std::isfinite(width) && width >= 1.0;
	}

} // namespace DICOMViewer
