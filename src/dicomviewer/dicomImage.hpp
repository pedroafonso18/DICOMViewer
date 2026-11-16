#pragma once

//--------------------------------------------

#include "dicomviewer/error.hpp"
#include "dicomviewer/frameContrast.hpp"
#include "dicomviewer/metadata.hpp"
#include "dicomviewer/renderedFrame.hpp"
#include "dicomviewer/window.hpp"

#include <filesystem>
#include <memory>
#include <optional>

//--------------------------------------------

namespace DICOMViewer {

	class Image {
	public:
		[[nodiscard]] static Result<Image> open(
			const std::filesystem::path& path
		);

		~Image();
		Image(Image&&) noexcept;
		Image& operator=(Image&&) noexcept;

		[[nodiscard]] const Metadata& metadata() const & noexcept;
		const Metadata& metadata() const && = delete;
		[[nodiscard]] FrameContrast contrastForFrame(unsigned long frame) const;
		// A missing window means a stored VOI LUT is active or the image is RGB.
		[[nodiscard]] std::optional<ContrastWindow> initialWindow() const;

		// A missing override restores the initial contrast.
		[[nodiscard]] Result<RenderedFrame> render(
			unsigned long frame = 0,
			std::optional<ContrastWindow> window = std::nullopt
		);

	private:
		struct ImageState;

		explicit Image(std::unique_ptr<ImageState> imageState) noexcept;
		std::unique_ptr<ImageState> state;
	};

}
