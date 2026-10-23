#ifdef TF2PL_COMPILE_LIBRARY
#include "filesystem_helpers.hpp"
#else
#define TF2PL_COMPILE_LIBRARY_INLINE inline
#endif

TF2PL_COMPILE_LIBRARY_INLINE std::filesystem::path tf2pl::filename_without_extension(std::filesystem::path path)
{
	return std::move(path.filename().replace_extension());
}

TF2PL_COMPILE_LIBRARY_INLINE std::filesystem::path tf2pl::replace_filename_keep_extension(std::filesystem::path path, std::filesystem::path newFilename)
{
	return std::move(path.replace_filename(newFilename.replace_extension(path.extension())));
}
