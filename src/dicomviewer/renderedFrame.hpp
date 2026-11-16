#pragma once

//--------------------------------------------

#include "dicomviewer/pixelFormat.hpp"

#include <cstdint>
#include <span>
#include <vector>

//--------------------------------------------

namespace DICOMViewer {

	class RenderedFrame {
	public:
		RenderedFrame(
			unsigned long width,
			unsigned long height,
			PixelFormat format,
			std::vector<std::uint8_t> pixels
		) noexcept;

		[[nodiscard]] unsigned long width() const noexcept;
		[[nodiscard]] unsigned long height() const noexcept;
		[[nodiscard]] PixelFormat format() const noexcept;
		[[nodiscard]] std::span<const std::uint8_t> pixels() const & noexcept;
		std::span<const std::uint8_t> pixels() const && = delete;

	private:
		unsigned long frameWidth;
		unsigned long frameHeight;
		PixelFormat pixelFormat;
		std::vector<std::uint8_t> pixelData;
	};

} // namespace DICOMViewer
