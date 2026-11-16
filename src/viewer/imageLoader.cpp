#include "viewer/imageLoader.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <utility>

//--------------------------------------------

namespace DICOMViewer {
	namespace {

		Result<QImage> renderImage(Image& session, const FrameRequest& request)
		{
			auto rendered = session.render(request.index, request.window);
			if (auto* error = std::get_if<Error>(&rendered)) {
				return std::move(*error);
			}

			const auto& frame = std::get<RenderedFrame>(rendered);
			const auto rowBytes = static_cast<std::size_t>(frame.width()) * channelCount(frame.format());
			constexpr auto maxDimension = static_cast<unsigned long>(std::numeric_limits<int>::max());
			if (rowBytes == 0 || frame.height() == 0 ||
				frame.width() > maxDimension || frame.height() > maxDimension ||
				frame.pixels().size() / rowBytes != frame.height() ||
				frame.pixels().size() % rowBytes != 0) {
				return Error{ErrorCode::RenderFailed, "Unexpected frame dimensions."};
			}

			const auto format = frame.format() == PixelFormat::Rgb8
				? QImage::Format_RGB888 : QImage::Format_Grayscale8;
			QImage image(static_cast<int>(frame.width()), static_cast<int>(frame.height()), format);
			if (image.isNull()) {
				return Error{ErrorCode::RenderFailed, "Could not allocate the display image."};
			}

			auto row = frame.pixels().begin();
			for (int y = 0; y < image.height(); ++y) {
				std::copy_n(row, rowBytes, image.scanLine(y));
				row += rowBytes;
			}
			return image;
		}

	} // namespace

	//--------------------------------------------

	Result<LoadedImage> ImageLoader::load(const QString& filePath) try
	{
		auto opened = Image::open(std::filesystem::path(filePath.toStdU16String()));
		if (auto* error = std::get_if<Error>(&opened)) {
			return std::move(*error);
		}

		auto& session = std::get<Image>(opened);
		auto rendered = renderImage(session, {});
		if (auto* error = std::get_if<Error>(&rendered)) {
			return std::move(*error);
		}

		LoadedImage snapshot{std::get<QImage>(std::move(rendered)), session.initialWindow(), session.metadata()};
		currentSession.emplace(std::move(session));
		return snapshot;
	}
	catch (const std::exception& error) {
		return Error{ErrorCode::RenderFailed, error.what()};
	}

	//--------------------------------------------

	Result<QImage> ImageLoader::render(const FrameRequest& request) try
	{
		if (!currentSession) {
			return Error{ErrorCode::RenderFailed, "No DICOM session is open."};
		}
		return renderImage(*currentSession, request);
	}
	catch (const std::exception& error) {
		return Error{ErrorCode::RenderFailed, error.what()};
	}

} // namespace DICOMViewer
