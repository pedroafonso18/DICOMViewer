#pragma once

//--------------------------------------------

#include <string>
#include <variant>

//--------------------------------------------

namespace DICOMViewer {

	enum class ErrorCode {
		FileAccess,
		InvalidDicom,
		MissingPixelData,
		IncompletePixelData,
		UnsupportedEncoding,
		UnsupportedPhotometricInterpretation,
		InvalidWindow,
		FrameOutOfRange,
		RenderFailed,
	};

	struct Error {
		ErrorCode code;
		std::string detail;
	};

	template<class T>
	using Result = std::variant<T, Error>;

} // namespace DICOMViewer
