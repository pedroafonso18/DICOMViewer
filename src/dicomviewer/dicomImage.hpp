#pragma once

//--------------------------------------------

#include "dicomviewer/error.hpp"
#include "dicomviewer/metadata.hpp"
#include "dicomviewer/renderedFrame.hpp"
#include "dicomviewer/window.hpp"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcvrus.h>
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
		// A missing window means a stored VOI LUT is active or the image is RGB.
		[[nodiscard]] std::optional<ContrastWindow> initialWindow() const noexcept;

		// A missing override restores the initial contrast.
		[[nodiscard]] Result<RenderedFrame> render(
			unsigned long frame = 0,
			std::optional<ContrastWindow> window = std::nullopt
		);

	private:
		struct ImageState {
			DcmFileFormat file;
			// DCMTK borrows the LUT data for the lifetime of the active transform.
			DcmUnsignedShort fractionalLut{DcmTag(DCM_LUTData, EVR_US)};
			std::unique_ptr<DicomImage> image;
			Metadata metadata;
			std::optional<ContrastWindow> initialWindow;
			std::optional<unsigned long> initialLut;
			unsigned long decodedFrame{};
		};

		explicit Image(std::unique_ptr<ImageState> imageState) noexcept;
		std::unique_ptr<ImageState> state;
	};

}
