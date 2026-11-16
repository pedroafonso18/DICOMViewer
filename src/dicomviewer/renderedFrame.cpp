#include "dicomviewer/renderedFrame.hpp"

#include <utility>

//--------------------------------------------

namespace DICOMViewer {

	RenderedFrame::RenderedFrame(
		unsigned long width,
		unsigned long height,
		std::vector<std::uint8_t> pixels
	) noexcept
		: frameWidth(width), frameHeight(height), pixelData(std::move(pixels))
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

	std::span<const std::uint8_t> RenderedFrame::pixels() const & noexcept
	{
		return pixelData;
	}

} // namespace DICOMViewer
