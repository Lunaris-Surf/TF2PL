#pragma once

#include <filesystem>

#ifndef TF2PL_API
#define TF2PL_API
#endif

namespace tf2pl
{
	TF2PL_API std::filesystem::path filename_without_extension(std::filesystem::path path);
	TF2PL_API std::filesystem::path replace_filename_keep_extension(std::filesystem::path path, std::filesystem::path newFilename);
}

#ifndef TF2PL_COMPILE_LIBRARY
#include "filesystem_helpers.inl"
#endif
