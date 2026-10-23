#include "HostAlias.h"
#include "Platform/Platform.h"
#include "Log.h"

#include <cerrno>
#include <fstream>
#include <iterator>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#include <objbase.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace
{
	std::filesystem::path HostsPath()
	{
#ifdef _WIN32
		wchar_t directory[MAX_PATH]{};
		const auto length = GetSystemDirectoryW(directory, MAX_PATH);
		if (!length || length >= MAX_PATH)
			throw std::runtime_error("couldn't locate the Windows system directory");
		return std::filesystem::path(directory) / "drivers" / "etc" / "hosts";
#else
		return "/etc/hosts";
#endif
	}

	tf2_bot_detector::WebHostAlias::State ReadState()
	{
		std::ifstream file(HostsPath(), std::ios::binary);
		if (!file)
			throw std::runtime_error("couldn't read the hosts file");
		std::string contents(std::istreambuf_iterator<char>{ file }, {});
		if (file.bad() || contents.size() > 4 * 1024 * 1024 || contents.find('\0') != contents.npos)
			throw std::runtime_error("unsupported hosts file size or encoding");
		return tf2_bot_detector::WebHostAlias::Inspect(contents);
	}

	bool RunSetup()
	{
		const auto helper = tf2_bot_detector::Platform::GetCurrentExeDir() / PLATFORM_EXECUTABLE("TF2PLHostSetup");
		if (!std::filesystem::is_regular_file(helper))
			throw std::runtime_error("TF2PLHostSetup is missing from the application folder");
#ifdef _WIN32
		const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		struct Uninit { HRESULT hr; ~Uninit() { if (SUCCEEDED(hr)) CoUninitialize(); } } uninit{ initialized };
		SHELLEXECUTEINFOW info{};
		info.cbSize = sizeof(info);
		info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
		info.lpVerb = L"runas";
		info.lpFile = helper.c_str();
		info.nShow = SW_HIDE;
		if (!ShellExecuteExW(&info) || !info.hProcess)
			return false; // includes a declined UAC prompt
		struct Close { HANDLE h; ~Close() { CloseHandle(h); } } close{ info.hProcess };
		DWORD exitCode = 1;
		return WaitForSingleObject(info.hProcess, INFINITE) == WAIT_OBJECT_0 &&
			GetExitCodeProcess(info.hProcess, &exitCode) && exitCode == 0;
#else
		// pkexec supplies the desktop's authentication prompt. No shell or password handling.
		std::string executable = helper.string();
		char pkexec[] = "/usr/bin/pkexec";
		char* args[] = { geteuid() == 0 ? executable.data() : pkexec,
			geteuid() == 0 ? nullptr : executable.data(), nullptr };
		pid_t child = 0;
		if (posix_spawn(&child, args[0], nullptr, nullptr, args, environ) != 0)
			return false;
		int status = 0;
		while (waitpid(child, &status, 0) < 0)
			if (errno != EINTR)
				return false;
		return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
	}
}

bool tf2_bot_detector::WebHostAlias::EnsureInstalled()
{
	try
	{
		const auto state = ReadState();
		if (state == State::Ready)
			return true;
		if (state == State::Conflict)
			throw std::runtime_error("local.tf2pl already has a conflicting hosts entry; leaving it unchanged");
		Log("Web UI: setting up local.tf2pl; approve the OS administrator prompt to add its loopback hosts entry.");
		if (RunSetup() && ReadState() == State::Ready)
			return true;
		LogWarning("Web UI: local.tf2pl setup was cancelled or unavailable; using 127.0.0.1. Linux setup requires pkexec and a desktop authentication agent.");
	}
	catch (const std::exception& e)
	{
		LogWarning("Web UI: {}. Using 127.0.0.1 instead.", e.what());
	}
	return false;
}
