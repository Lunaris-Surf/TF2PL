#pragma once

#ifdef _WIN32

#include "ITF2BotDetectorRenderer.h"

#include <memory>
#include <string>
#include <vector>

struct HWND__;
struct ImDrawData;

/// <summary>
/// Direct3D 11 device + swapchain for the SDL window.
/// Kept separate so d3d11/dxgi headers don't leak into the rest of the renderer.
/// </summary>
class D3D11Backend
{
public:
	D3D11Backend();
	~D3D11Backend();

	/// <summary>
	/// GPUs usable for rendering, best (high performance) first. Software adapters are skipped.
	/// </summary>
	static std::vector<TF2BDGpuInfo> EnumerateGpus();

	/// <summary>
	/// create the device on the preferred GPU (empty id = prefer discrete), and a swapchain for hwnd.
	/// also initializes the imgui DX11 backend. returns false if D3D11 isn't usable.
	/// </summary>
	bool Init(HWND__* hwnd, const std::string& preferredGpuID);
	void Shutdown();

	void NewFrame();
	void Render(ImDrawData* drawData, const float clearColor[4]);

	/// <summary>
	/// returns false if the window is fully occluded (nothing to show, caller may sleep).
	/// </summary>
	bool Present(bool vsync);

	void OnResize();

	void* CreateTextureRGBA(const void* rgba, uint32_t width, uint32_t height);
	// static: safe to call after Shutdown (the view holds its own COM reference)
	static void DestroyTexture(void* texture);

	const TF2BDGpuInfo& GetActiveGpu() const { return m_ActiveGpu; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_Impl;
	TF2BDGpuInfo m_ActiveGpu;
};

#endif
