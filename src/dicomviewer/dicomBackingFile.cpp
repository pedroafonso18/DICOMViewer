#include "dicomviewer/dicomBackingFile.hpp"

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#else
#include <fstream>
#endif

//--------------------------------------------

namespace DICOMViewer {

	DicomBackingFile::~DicomBackingFile()
	{
#ifdef __linux__
		if (descriptor >= 0) {
			::close(descriptor);
		}
#endif
	}

	//--------------------------------------------

	std::optional<Error> DicomBackingFile::open(const std::filesystem::path& path)
	{
#ifdef __linux__
		descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
		if (descriptor < 0 || ::fstat(descriptor, &original) != 0 || !S_ISREG(original.st_mode)) {
			return Error{ErrorCode::FileAccess, "Cannot read the input file."};
		}

		// DCMTK reopens its input path for deferred reads. This path names the
		// open file descriptor, so replacing the original pathname cannot redirect it.
		backingPath = std::filesystem::path("/proc/self/fd") / std::to_string(descriptor);
#else
		std::error_code filesystemError;
		if (!std::filesystem::is_regular_file(path, filesystemError)) {
			return Error{ErrorCode::FileAccess, "Cannot read the input file."};
		}
		snapshot = std::make_unique<OFTempFile>();
		if (snapshot->getStatus().bad()) {
			return Error{ErrorCode::FileAccess, "Cannot create a temporary DICOM backing file."};
		}
		backingPath = snapshot->getFilename();
		std::ifstream source(path, std::ios::binary);
		std::ofstream destination(backingPath, std::ios::binary);
		if (!source || !destination || !(destination << source.rdbuf()) || source.bad()) {
			return Error{ErrorCode::FileAccess, "Cannot copy the input file."};
		}
		destination.close();
		if (!destination) {
			return Error{ErrorCode::FileAccess, "Cannot finish writing the temporary DICOM file."};
		}
#endif
		return std::nullopt;
	}

	//--------------------------------------------

	const std::filesystem::path& DicomBackingFile::readPath() const noexcept
	{
		return backingPath;
	}

	//--------------------------------------------

	std::optional<Error> DicomBackingFile::checkUnchanged() const
	{
#ifdef __linux__
		struct stat current{};
		if (::fstat(descriptor, &current) != 0 || original.st_size != current.st_size ||
			original.st_mtim.tv_sec != current.st_mtim.tv_sec ||
			original.st_mtim.tv_nsec != current.st_mtim.tv_nsec ||
			original.st_ctim.tv_sec != current.st_ctim.tv_sec ||
			original.st_ctim.tv_nsec != current.st_ctim.tv_nsec) {
			return Error{ErrorCode::FileAccess, "The backing file changed after opening. Reopen it to continue."};
		}
#endif
		return std::nullopt;
	}

} // namespace DICOMViewer
