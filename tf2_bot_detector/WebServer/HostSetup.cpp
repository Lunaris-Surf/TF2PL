// Standalone elevated helper: no app DLLs, networking, scripts, or user-supplied
// paths/hostnames. Its sole write is appending the fixed TF2PL loopback entry.
#include "HostAlias.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
	constexpr size_t MAX_HOSTS_SIZE = 4 * 1024 * 1024;

	std::string Addition(const std::string& contents)
	{
		return (contents.empty() || contents.back() == '\n' ? "" : "\n") +
			std::string(tf2_bot_detector::WebHostAlias::ENTRY) + "\n";
	}
}

int main(int argc, char**)
{
	if (argc != 1)
		return 1;

#ifdef _WIN32
	wchar_t systemDir[MAX_PATH]{};
	const auto length = GetSystemDirectoryW(systemDir, MAX_PATH);
	if (!length || length >= MAX_PATH)
		return 1;
	const std::wstring path = std::wstring(systemDir) + L"\\drivers\\etc\\hosts";
	// Deny concurrent writers while checking and appending. Do not create a new hosts file.
	HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
		nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return 1;
	struct Close { HANDLE h; ~Close() { CloseHandle(h); } } close{ file };
	LARGE_INTEGER size{};
	if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > static_cast<LONGLONG>(MAX_HOSTS_SIZE))
		return 1;
	std::string contents(static_cast<size_t>(size.QuadPart), '\0');
	DWORD read = 0;
	if (!ReadFile(file, contents.data(), static_cast<DWORD>(contents.size()), &read, nullptr) || read != contents.size())
		return 1;
#else
	const int file = open("/etc/hosts", O_RDWR | O_CLOEXEC | O_NOFOLLOW);
	if (file < 0)
		return 1;
	struct Close { int fd; ~Close() { ::close(fd); } } close{ file };
	if (flock(file, LOCK_EX) != 0)
		return 1;
	struct stat info{};
	if (fstat(file, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
		static_cast<uint64_t>(info.st_size) > MAX_HOSTS_SIZE)
		return 1;
	std::string contents(static_cast<size_t>(info.st_size), '\0');
	size_t offset = 0;
	while (offset < contents.size())
	{
		const auto count = read(file, contents.data() + offset, contents.size() - offset);
		if (count <= 0)
			return 1;
		offset += static_cast<size_t>(count);
	}
#endif
	using namespace tf2_bot_detector::WebHostAlias;
	const auto state = Inspect(contents);
	if (state == State::Ready)
		return 0;
	if (state == State::Conflict || contents.find('\0') != contents.npos)
		return 2; // don't replace existing mappings or rewrite an unsupported encoding
	const auto addition = Addition(contents);
#ifdef _WIN32
	LARGE_INTEGER end{};
	if (!SetFilePointerEx(file, end, nullptr, FILE_END))
		return 1;
	DWORD written = 0;
	if (!WriteFile(file, addition.data(), static_cast<DWORD>(addition.size()), &written, nullptr) || written != addition.size())
		return 1;
	return FlushFileBuffers(file) ? 0 : 1;
#else
	if (lseek(file, 0, SEEK_END) < 0)
		return 1;
	offset = 0;
	while (offset < addition.size())
	{
		const auto count = write(file, addition.data() + offset, addition.size() - offset);
		if (count <= 0)
			return 1;
		offset += static_cast<size_t>(count);
	}
	return fsync(file) == 0 ? 0 : 1;
#endif
}
