#pragma once

//--------------------------------------------

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
			std::vector<std::uint8_t> pixels
		) noexcept;

		[[nodiscard]] unsigned long width() const noexcept;
		[[nodiscard]] unsigned long height() const noexcept;
		[[nodiscard]] std::span<const std::uint8_t> pixels() const & noexcept;
		std::span<const std::uint8_t> pixels() const && = delete;

	private:
		unsigned long frameWidth;
		unsigned long frameHeight;
		std::vector<std::uint8_t> pixelData;
	};

} // namespace DICOMViewer
