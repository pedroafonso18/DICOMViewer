#include "dicomviewer/renderedFrame.hpp"

#include <utility>

//--------------------------------------------

namespace DICOMViewer {

	RenderedFrame::RenderedFrame(
		unsigned long width,
		unsigned long height,
		PixelFormat format,
		std::vector<std::uint8_t> pixels
	) noexcept
		: frameWidth(width), frameHeight(height), pixelFormat(format), pixelData(std::move(pixels))
	{
	}

	//--------------------------------------------

	unsigned long RenderedFrame::width() const noexcept
	{
		return frameWidth;
	}

	//--------------------------------------------

	unsigned long RenderedFrame::height() const noexcept
	{
		return frameHeight;
	}

	//--------------------------------------------

	PixelFormat RenderedFrame::format() const noexcept
	{
		return pixelFormat;
	}

	//--------------------------------------------

	std::span<const std::uint8_t> RenderedFrame::pixels() const & noexcept
	{
		return pixelData;
	}

} // namespace DICOMViewer
