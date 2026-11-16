#pragma once

//--------------------------------------------

namespace DICOMViewer {

	struct ContrastWindow {
		double windowLevel;
		double width;

		[[nodiscard]] bool isValid() const noexcept;
	};

} // namespace DICOMViewer
