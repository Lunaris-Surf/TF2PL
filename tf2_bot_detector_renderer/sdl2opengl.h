#pragma once
#include "ITF2BotDetectorRenderer.h"
#include <SDL.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
class D3D11Backend;
#endif

struct SDL_Window;
struct ImGuiContext;
struct ImFontAtlas;

class ITF2BotDetectorDrawable;

/// <summary>
/// SDL2 window + Dear ImGui, drawn with Direct3D 11 (Windows default) or OpenGL 4.3.
///
/// currently external window only
/// </summary>
class TF2BotDetectorSDLRenderer : public TF2BotDetectorRendererBase {
public:
	explicit TF2BotDetectorSDLRenderer(const TF2BDRendererOptions& options = {});
	~TF2BotDetectorSDLRenderer();

	/// <summary>
	/// ImGui::Render() related functions.
	/// in overlay this would be somewhere in EndScene.
	/// </summary>
	void DrawFrame();

	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	std::size_t RegisterDrawCallback(DrawableCallbackFn);

	/// <summary>
	/// Should we stop running and destroy?
	/// </summary>
	/// <returns></returns>
	bool ShouldQuit() const;

	/// <summary>
	/// Request application destruction.
	/// </summary>
	/// <returns></returns>
	void RequestQuit();

	/// <summary>
	/// how frequent should DrawFrame run?
	///
	/// note: ignored in overlay mode.
	/// </summary>
	/// <param name="frameTime">frame time in ms</param>
	void SetFramerate(float);
	float GetFramerate() const;

	/// <summary>
	/// determines if the user is actively interacting with our window. 
	/// </summary>
	/// <returns></returns>
	bool InFocus() const;

	std::string RendererInfo() const;
	void SetGameOverlayEnabled(bool enabled);
	void SetGameOverlayHotkey(int key, int modifiers);
	bool IsGameOverlayEnabled() const override { return gameOverlayApplied; }
	bool IsGameOverlayInteractive() const override { return gameOverlayApplied && gameOverlayInteractive; }

	TF2BDRenderBackend GetBackend() const override { return backend; }
	void SetRendererSettingsOpen(bool open) override { rendererSettingsOpen = open; }
	std::vector<TF2BDGpuInfo> GetAvailableGpus() const override;
	TF2BDGpuInfo GetActiveGpu() const override { return activeGpu; }

	void* CreateTextureRGBA(const void* rgba, uint32_t width, uint32_t height) override;
	void DestroyTexture(void* texture) override;

private:
	void UpdateGameOverlay();

	bool InitOpenGL();
#ifdef _WIN32
	bool InitD3D11(const std::string& preferredGpuID);
	std::unique_ptr<D3D11Backend> d3d11;
#endif
	void DestroyWindowAndBackend();
	void SetClearColor(float r, float g, float b, float a);

	TF2BDRenderBackend backend = TF2BDRenderBackend::OpenGL;
	TF2BDGpuInfo activeGpu;
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	bool windowOccluded = false;
	bool rendererSettingsOpen = false;

	std::vector<DrawableCallbackFn> drawFunctions;

	SDL_Window* window = nullptr;
	SDL_GLContext gl_context = nullptr;

	bool running = true;
	// runs 60fps by default
	float frameTime = 16.6f;
	bool gameOverlayEnabled = false;
	bool gameOverlayApplied = false;
	bool gameOverlayInteractive = false;
	bool overlayHotkeyWasDown = false;
	int overlayHotkeyKey = 0x09;
	int overlayHotkeyModifiers = 1;
	int normalX = 0;
	int normalY = 0;
	int normalWidth = 1280;
	int normalHeight = 720;

	// overlay window tracking, so we only touch window state when something actually changes.
	// calling SetWindowPos(HWND_TOPMOST) every frame forces DWM to recomposite over TF2.
	void* cachedTF2Window = nullptr;     // HWND
	uint64_t lastTF2WindowLookupMs = 0;
	bool overlayHidden = false;
	int overlayLastX = 0;
	int overlayLastY = 0;
	int overlayLastW = 0;
	int overlayLastH = 0;
	bool overlayLastInteractive = false;
	bool overlayPlacementValid = false;

	// timestamp (SDL_GetTicks64) of when the current frame started, for frame pacing.
	uint64_t frameStartMs = 0;
};
