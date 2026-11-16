#include "viewer/imageLoader.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <utility>

//--------------------------------------------

namespace DICOMViewer {

	Result<LoadedImage> ImageLoader::load(const QString& filePath) try
	{
		auto opened = Image::open(std::filesystem::path(filePath.toStdU16String()));
		if (auto* error = std::get_if<Error>(&opened)) {
			return std::move(*error);
		}

		auto& session = std::get<Image>(opened);
		auto rendered = session.render();
		if (auto* error = std::get_if<Error>(&rendered)) {
			return std::move(*error);
		}

		const auto& frame = std::get<RenderedFrame>(rendered);
		constexpr auto maxDimension = static_cast<unsigned long>(std::numeric_limits<int>::max());
		if (frame.width() == 0 || frame.height() == 0 ||
			frame.width() > maxDimension || frame.height() > maxDimension ||
			frame.pixels().size() / frame.width() != frame.height() ||
			frame.pixels().size() % frame.width() != 0) {
			return Error{ErrorCode::RenderFailed, "Unexpected grayscale frame dimensions."};
		}

		const auto width = static_cast<int>(frame.width());
		const auto height = static_cast<int>(frame.height());
		QImage image(width, height, QImage::Format_Grayscale8);
		if (image.isNull()) {
			return Error{ErrorCode::RenderFailed, "Could not allocate the display image."};
		}

		auto row = frame.pixels().begin();
		for (int y = 0; y < height; ++y) {
			std::copy_n(row, width, image.scanLine(y));
			row += width;
		}
		LoadedImage snapshot{std::move(image), session.metadata()};
		currentSession.emplace(std::move(session));
		return snapshot;
	}
	catch (const std::exception& error) {
		return Error{ErrorCode::RenderFailed, error.what()};
	}

} // namespace DICOMViewer
