#pragma once

//--------------------------------------------

namespace DICOMViewer {

	enum class PixelFormat { Grayscale8, Rgb8 };

	[[nodiscard]] constexpr unsigned int channelCount(PixelFormat format) noexcept
	{
		constexpr unsigned int grayscaleChannels = 1;
		constexpr unsigned int rgbChannels = 3;
		return format == PixelFormat::Rgb8 ? rgbChannels : grayscaleChannels;
	}

} // namespace DICOMViewer
