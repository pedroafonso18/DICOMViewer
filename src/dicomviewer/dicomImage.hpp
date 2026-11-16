#pragma once

//--------------------------------------------

#include "dicomviewer/error.hpp"
#include "dicomviewer/metadata.hpp"
#include "dicomviewer/renderedFrame.hpp"
#include "dicomviewer/window.hpp"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmimgle/dcmimage.h>

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

		[[nodiscard]] const Metadata& metadata() const noexcept;
		[[nodiscard]] ContrastWindow initialWindow() const noexcept;

		[[nodiscard]] Result<RenderedFrame> render(
			unsigned long frame = 0,
			std::optional<ContrastWindow> window = std::nullopt
		);

	private:
		struct ImageState {
			DcmFileFormat file;
			std::unique_ptr<DicomImage> image;
			Metadata metadata;
			ContrastWindow initialWindow{};
		};

		explicit Image(std::unique_ptr<ImageState> imageState) noexcept;
		std::unique_ptr<ImageState> state;
	};

}
