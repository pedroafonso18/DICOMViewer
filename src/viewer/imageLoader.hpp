#pragma once

//--------------------------------------------

#include "dicomviewer/dicomImage.hpp"
#include "viewer/loadedImage.hpp"
#include "viewer/frameRequest.hpp"

#include <QString>

#include <optional>

//--------------------------------------------

namespace DICOMViewer {

	class ImageLoader {
	public:
		[[nodiscard]] Result<LoadedImage> load(const QString& filePath);
		[[nodiscard]] Result<RenderedImage> render(const FrameRequest& request);

	private:
		std::optional<Image> currentSession;
	};

} // namespace DICOMViewer
