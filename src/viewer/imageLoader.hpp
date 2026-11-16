#pragma once

//--------------------------------------------

#include "dicomviewer/dicomImage.hpp"
#include "viewer/loadedImage.hpp"

#include <QString>

#include <optional>

//--------------------------------------------

namespace DICOMViewer {

	class ImageLoader {
	public:
		[[nodiscard]] Result<LoadedImage> load(const QString& filePath);

	private:
		std::optional<Image> currentSession;
	};

} // namespace DICOMViewer
