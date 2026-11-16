#pragma once

//--------------------------------------------

#include "dicomviewer/error.hpp"

#include <filesystem>
#include <optional>

#ifdef __linux__
#include <sys/stat.h>
#else
#include <dcmtk/config/osconfig.h>
#include <dcmtk/ofstd/oftempf.h>
#include <memory>
#endif

//--------------------------------------------

namespace DICOMViewer {

	class DicomBackingFile {
	public:
		DicomBackingFile() = default;
		~DicomBackingFile();
		DicomBackingFile(const DicomBackingFile&) = delete;
		DicomBackingFile& operator=(const DicomBackingFile&) = delete;

		[[nodiscard]] std::optional<Error> open(const std::filesystem::path& path);
		[[nodiscard]] const std::filesystem::path& readPath() const noexcept;
		[[nodiscard]] std::optional<Error> checkUnchanged() const;

	private:
		std::filesystem::path backingPath;
#ifdef __linux__
		int descriptor{-1};
		struct stat original{};
#else
		std::unique_ptr<OFTempFile> snapshot;
#endif
	};

} // namespace DICOMViewer
