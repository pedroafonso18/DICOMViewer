#pragma once

//--------------------------------------------

namespace DICOMViewer {

	enum class VoiFunction { Linear, LinearExact, Sigmoid };

	struct ContrastWindow {
		static constexpr double minimumLinearWidth = 1.0;

		double windowLevel;
		double width;
		VoiFunction function{VoiFunction::Linear};

		[[nodiscard]] bool isValid() const noexcept;
	};

} // namespace DICOMViewer
