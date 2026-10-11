#include "sdl2opengl.h"

#if WIN32
#include <Windows.h>
#endif

#include <glad/gl.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_opengl.h>

#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>

#include "UI/AppStyle.h"

#ifdef _WIN32
#include "d3d11_backend.h"
#include <backends/imgui_impl_dx11.h>
#endif

#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <fmt/format.h>
#include <fmt/compile.h>

#define GL_APIENTRY GLAD_API_PTR

using namespace std::string_literals;

static std::string version_string = fmt::format(FMT_COMPILE("LunarisV v{} (External)"), BD_VERSION);

TF2BotDetectorSDLRenderer::TF2BotDetectorSDLRenderer(const TF2BDRendererOptions& options) : TF2BotDetectorRendererBase()
{
	// Setup SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
	{
		printf("Error: %s\n", SDL_GetError());
		// TODO: crash
	}

	bool initialized = false;

#ifdef _WIN32
	if (options.m_Backend == TF2BDRenderBackend::D3D11)
	{
		initialized = InitD3D11(options.m_PreferredGpuID);
		if (!initialized)
		{
			OutputDebugStringA("[!] Direct3D 11 init failed, falling back to OpenGL\n");
			DestroyWindowAndBackend();
		}
	}
#endif

	if (!initialized)
		initialized = InitOpenGL();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// https://github.com/ocornut/imgui/issues/2117
	// multi window causes problems in linux, apparently; investigate?
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
#ifndef TF2BD_IMGUI_NO_MUTLI_VIEWPORTS
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
#endif

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	tf2_bot_detector::ApplyModernDarkTheme();

	// Setup Platform/Renderer backends
	if (backend == TF2BDRenderBackend::OpenGL)
	{
		ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
		ImGui_ImplOpenGL3_Init("#version 430 core");
	}
	else
	{
		// the DX11 imgui backend is initialized by D3D11Backend on the first NewFrame
		ImGui_ImplSDL2_InitForD3D(window);
	}

	// show our window cuz SDL_WINDOW_HIDDEN funny
	SDL_ShowWindow(window);
}

// SDL_WINDOW_HIDDEN: Why? Why do I have to do this or it complains constantly about IID_ITfUIElementSink not being registered, why???
// - a born-visible window gets its first WM_SETFOCUS synchronously inside SDL_CreateWindow, which kicks SDL's TSF/IME setup
//   before that subsystem is fully initialized. so windows are created hidden and shown at the end of the constructor.
static SDL_Window* CreateMainWindow(SDL_WindowFlags apiFlag)
{
	const auto window_flags = (SDL_WindowFlags)(apiFlag | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN);
	SDL_Window* window = SDL_CreateWindow(version_string.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, window_flags);
	if (!window)
		printf("[!] SDL_CreateWindow failed! (SDL_GetError(): %s)!\n", SDL_GetError());
	return window;
}

bool TF2BotDetectorSDLRenderer::InitOpenGL()
{
	backend = TF2BDRenderBackend::OpenGL;

	// * GL 4.3 + GLSL 430 core *
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	window = CreateMainWindow(SDL_WINDOW_OPENGL);
	gl_context = SDL_GL_CreateContext(window);

	// uh oh.
	if (!gl_context) {
		printf("[!] gl_context is null (SDL_GetError(): %s)!\n", SDL_GetError());
		return false;
	}

	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(1); // Enable vsync

#ifdef IMGUI_USE_GLAD2
	if (!gladLoadGL( [](const char* name) 
#ifdef _WIN32
	__declspec(noinline)
#else
	__attribute__((noinline))
#endif
	{
		return reinterpret_cast<GLADapiproc>(SDL_GL_GetProcAddress(name));
	})) {
#ifdef _WIN32
	OutputDebugString(
#else
	printf(
#endif
		"[!] gladLoadGL returned 0! we will probably crash!\n");
	}
#endif

	if (const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER)))
		activeGpu.m_Name = renderer;

	return true;
}

#ifdef _WIN32
bool TF2BotDetectorSDLRenderer::InitD3D11(const std::string& preferredGpuID)
{
	backend = TF2BDRenderBackend::D3D11;

	window = CreateMainWindow((SDL_WindowFlags)0);
	if (!window)
		return false;

	SDL_SysWMinfo info{};
	SDL_VERSION(&info.version);
	if (!SDL_GetWindowWMInfo(window, &info))
		return false;

	d3d11 = std::make_unique<D3D11Backend>();
	if (!d3d11->Init(info.info.win.window, preferredGpuID))
	{
		d3d11.reset();
		return false;
	}

	activeGpu = d3d11->GetActiveGpu();
	return true;
}
#endif

void TF2BotDetectorSDLRenderer::DestroyWindowAndBackend()
{
#ifdef _WIN32
	d3d11.reset();
#endif
	if (gl_context)
	{
		SDL_GL_DeleteContext(gl_context);
		gl_context = nullptr;
	}
	if (window)
	{
		SDL_DestroyWindow(window);
		window = nullptr;
	}
}

void TF2BotDetectorSDLRenderer::SetClearColor(float r, float g, float b, float a)
{
	clearColor[0] = r;
	clearColor[1] = g;
	clearColor[2] = b;
	clearColor[3] = a;
}

std::vector<TF2BDGpuInfo> TF2BotDetectorSDLRenderer::GetAvailableGpus() const
{
#ifdef _WIN32
	return D3D11Backend::EnumerateGpus();
#else
	return {};
#endif
}

void* TF2BotDetectorSDLRenderer::CreateTextureRGBA(const void* rgba, uint32_t width, uint32_t height)
{
#ifdef _WIN32
	if (d3d11)
		return d3d11->CreateTextureRGBA(rgba, width, height);
#endif

	GLuint handle = 0;
	glGenTextures(1, &handle);
	if (!handle)
		return nullptr;

	glBindTexture(GL_TEXTURE_2D, handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
		GL_RGBA, GL_UNSIGNED_BYTE, rgba);

	return reinterpret_cast<void*>(static_cast<uintptr_t>(handle));
}

void TF2BotDetectorSDLRenderer::DestroyTexture(void* texture)
{
	if (!texture)
		return;

#ifdef _WIN32
	if (backend == TF2BDRenderBackend::D3D11)
	{
		D3D11Backend::DestroyTexture(texture);
		return;
	}
#endif

	// textures can outlive the GL context during shutdown; the context took them with it.
	if (!gl_context)
		return;

	const GLuint handle = static_cast<GLuint>(reinterpret_cast<uintptr_t>(texture));
	glDeleteTextures(1, &handle);
}

void TF2BotDetectorSDLRenderer::SetGameOverlayEnabled(bool enabled)
{
	gameOverlayEnabled = enabled;
}

void TF2BotDetectorSDLRenderer::SetGameOverlayHotkey(int key, int modifiers)
{
	overlayHotkeyKey = key;
	overlayHotkeyModifiers = modifiers;
}

void TF2BotDetectorSDLRenderer::UpdateGameOverlay()
{
#ifdef _WIN32
	SDL_SysWMinfo info{};
	SDL_VERSION(&info.version);
	if (!SDL_GetWindowWMInfo(window, &info))
		return;
	HWND ownWindow = info.info.win.window;

	// FindWindowA walks every top-level window; only do it about once a second,
	// or straight away if the window we cached has gone away.
	const uint64_t nowMs = SDL_GetTicks64();
	HWND tf2Window = static_cast<HWND>(cachedTF2Window);
	if (!tf2Window || !IsWindow(tf2Window) || (nowMs - lastTF2WindowLookupMs) >= 1000)
	{
		tf2Window = FindWindowA("Valve001", nullptr);
		cachedTF2Window = tf2Window;
		lastTF2WindowLookupMs = nowMs;
	}

	if (!gameOverlayEnabled || !tf2Window)
	{
		if (gameOverlayApplied)
		{
			LONG_PTR exStyle = GetWindowLongPtrW(ownWindow, GWL_EXSTYLE);
			SetWindowLongPtrW(ownWindow, GWL_EXSTYLE,
				exStyle & ~(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE));
			SDL_SetWindowBordered(window, SDL_TRUE);
			SDL_SetWindowAlwaysOnTop(window, SDL_FALSE);
			SDL_SetWindowPosition(window, normalX, normalY);
			SDL_SetWindowSize(window, normalWidth, normalHeight);
			if (overlayHidden)
				ShowWindow(ownWindow, SW_SHOW);
			SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			gameOverlayApplied = false;
			gameOverlayInteractive = false;
			overlayHotkeyWasDown = false;
			overlayHidden = false;
			overlayPlacementValid = false;
		}
		return;
	}

	RECT client{};
	POINT origin{};
	if (!GetClientRect(tf2Window, &client) || !ClientToScreen(tf2Window, &origin))
		return;
	const int width = client.right - client.left;
	const int height = client.bottom - client.top;
	if (width <= 0 || height <= 0)
		return;
	const HWND foreground = GetForegroundWindow();
	if (IsIconic(tf2Window) || (foreground != tf2Window && foreground != ownWindow))
	{
		if (!overlayHidden)
		{
			ShowWindow(ownWindow, SW_HIDE);
			overlayHidden = true;
		}
		return;
	}

	if (!gameOverlayApplied)
	{
		SDL_GetWindowPosition(window, &normalX, &normalY);
		SDL_GetWindowSize(window, &normalWidth, &normalHeight);
		SDL_SetWindowBordered(window, SDL_FALSE);
		SDL_SetWindowAlwaysOnTop(window, SDL_TRUE);
		LONG_PTR exStyle = GetWindowLongPtrW(ownWindow, GWL_EXSTYLE);
		SetWindowLongPtrW(ownWindow, GWL_EXSTYLE,
			exStyle | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
		SetLayeredWindowAttributes(ownWindow, RGB(255, 0, 255), 0, LWA_COLORKEY);
		SetClearColor(1.0f, 0.0f, 1.0f, 0.0f);
		gameOverlayApplied = true;
		gameOverlayInteractive = false;
		overlayPlacementValid = false;
	}

	const bool controlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
	const bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
	const bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
	const bool modifiersDown = (!(overlayHotkeyModifiers & 1) || controlDown) &&
		(!(overlayHotkeyModifiers & 2) || altDown) &&
		(!(overlayHotkeyModifiers & 4) || shiftDown);
	const bool hotkeyDown = overlayHotkeyKey > 0 && modifiersDown &&
		(GetAsyncKeyState(overlayHotkeyKey) & 0x8000) != 0;
	if (hotkeyDown && !overlayHotkeyWasDown)
	{
		gameOverlayInteractive = !gameOverlayInteractive;
		LONG_PTR exStyle = GetWindowLongPtrW(ownWindow, GWL_EXSTYLE);
		if (gameOverlayInteractive)
		{
			// Color-keyed pixels in a layered window are also transparent to hit testing.
			// Remove the layered style while interactive so the full client area consumes
			// mouse input instead of allowing clicks through to TF2.
			exStyle &= ~(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
			SetWindowLongPtrW(ownWindow, GWL_EXSTYLE, exStyle);
			SetWindowPos(ownWindow, HWND_TOPMOST, origin.x, origin.y, width, height,
				SWP_SHOWWINDOW | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			SetClearColor(0.035f, 0.035f, 0.055f, 1.0f);
			BringWindowToTop(ownWindow);
			SetForegroundWindow(ownWindow);
			SetActiveWindow(ownWindow);
			SetFocus(ownWindow);
		}
		else
		{
			exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
			SetWindowLongPtrW(ownWindow, GWL_EXSTYLE, exStyle);
			SetLayeredWindowAttributes(ownWindow, RGB(255, 0, 255), 0, LWA_COLORKEY);
			SetWindowPos(ownWindow, HWND_TOPMOST, origin.x, origin.y, width, height,
				SWP_SHOWWINDOW | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
			SetClearColor(1.0f, 0.0f, 1.0f, 0.0f);
			SetForegroundWindow(tf2Window);
		}
		overlayHidden = false;
		overlayLastX = origin.x;
		overlayLastY = origin.y;
		overlayLastW = width;
		overlayLastH = height;
		overlayLastInteractive = gameOverlayInteractive;
		overlayPlacementValid = true;
	}
	overlayHotkeyWasDown = hotkeyDown;

	// Only reposition when TF2 moved/resized, we were hidden, or the mode changed.
	const bool placementChanged = !overlayPlacementValid || overlayHidden ||
		origin.x != overlayLastX || origin.y != overlayLastY ||
		width != overlayLastW || height != overlayLastH ||
		gameOverlayInteractive != overlayLastInteractive;
	if (placementChanged)
	{
		SetWindowPos(ownWindow, HWND_TOPMOST, origin.x, origin.y, width, height,
			SWP_SHOWWINDOW | SWP_NOOWNERZORDER | (gameOverlayInteractive ? 0 : SWP_NOACTIVATE));
		overlayHidden = false;
		overlayLastX = origin.x;
		overlayLastY = origin.y;
		overlayLastW = width;
		overlayLastH = height;
		overlayLastInteractive = gameOverlayInteractive;
		overlayPlacementValid = true;
	}
#else
	gameOverlayEnabled = false;
#endif
}

TF2BotDetectorSDLRenderer::~TF2BotDetectorSDLRenderer()
{
	// Cleanup
#ifdef _WIN32
	if (d3d11)
		d3d11->Shutdown(); // shuts down the imgui DX11 backend, must happen before DestroyContext
#endif
	if (backend == TF2BDRenderBackend::OpenGL)
		ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	DestroyWindowAndBackend();
	SDL_Quit();
}

void TF2BotDetectorSDLRenderer::DrawFrame()
{
	UpdateGameOverlay();

	// When nobody is looking at us, don't render at full rate next to TF2.
	// Wait for input (or 100ms, so app updates/RCON still tick ~10x a second).
	// SDL_WaitEventTimeout(nullptr, ...) does not remove the event, the poll loop below handles it.
	{
		const bool overlayHiddenNow = gameOverlayApplied && overlayHidden;
		const bool unfocusedWindow = !gameOverlayApplied && !InFocus();
		if (overlayHiddenNow || unfocusedWindow || windowOccluded)
			SDL_WaitEventTimeout(nullptr, 100);
	}

	frameStartMs = SDL_GetTicks64();
	// Poll and handle events (inputs, window resize, etc.)
	// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
	// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
	// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
	// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		ImGui_ImplSDL2_ProcessEvent(&event);
		if (event.type == SDL_QUIT)
			running = false;
		if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
			running = false;
#ifdef _WIN32
		if (d3d11 && event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
			event.window.windowID == SDL_GetWindowID(window))
		{
			d3d11->OnResize();
		}
#endif
	}

	// Start the Dear ImGui frame
#ifdef _WIN32
	if (d3d11)
		d3d11->NewFrame();
	else
#endif
		ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();

	// draw our registered draw functions
	{
		for (size_t i = 0; i < drawFunctions.size(); i++) {
			const auto& callOnDraw = drawFunctions.at(i);
			callOnDraw();
		}
	}

	// opened from View > Renderer stats (the app's header bar owns the menu bar)
	if (rendererSettingsOpen && !gameOverlayApplied) {
	if (ImGui::Begin("Renderer Settings", &rendererSettingsOpen)) {
		ImGui::Text("Note: This setting will still prefer vSync.");
		ImGui::TextUnformatted(RendererInfo().c_str());

		ImGui::SliderFloat("Set Frame Time", &frameTime, 0.0f, 66.6f);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

#ifdef _DEBUG
		if (!this->InFocus()) {
			ImGui::Text("Application State: <Sleeping>");
		}
		else {
			ImGui::Text("Application State: <Active>");
		}
#endif // _DEBUG
	}
	ImGui::End(); // must pair with Begin even when collapsed
	}


	// Rendering
	ImGui::Render();
#ifdef _WIN32
	if (d3d11)
	{
		d3d11->Render(ImGui::GetDrawData(), clearColor);

		// Update and Render additional Platform Windows
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		windowOccluded = !d3d11->Present(true);
	}
	else
#endif
	{
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Update and Render additional Platform Windows
		// (Platform functions may change the current OpenGL context, so we save/restore it)
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
			SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
		}

		SDL_GL_SwapWindow(window);
	}

	// Frame pacing: only sleep for whatever is left of the frame budget.
	// (Previously this always slept the full frameTime on top of vsync, which halved the framerate.)
	{
		float budgetMs = this->frameTime;

		// The passive (click-through) overlay is just a list of names; ~20 FPS is plenty
		// and leaves the GPU to TF2.
		if (gameOverlayApplied && !gameOverlayInteractive && budgetMs < 50.0f)
			budgetMs = 50.0f;

		const uint64_t elapsedMs = SDL_GetTicks64() - frameStartMs;
		if (budgetMs > 0.0f && static_cast<float>(elapsedMs) < budgetMs)
			SDL_Delay(static_cast<Uint32>(budgetMs - static_cast<float>(elapsedMs)));
	}
}

std::size_t TF2BotDetectorSDLRenderer::RegisterDrawCallback(DrawableCallbackFn function)
{
	drawFunctions.push_back(function);
	return drawFunctions.size();
}

bool TF2BotDetectorSDLRenderer::ShouldQuit() const
{
	return !running;
}

void TF2BotDetectorSDLRenderer::RequestQuit()
{
	running = false;
}

/// <summary>
/// sets frame time, so we can limit how much we render (60fps is a good target)
/// </summary>
/// <param name="newFrameTime"></param>
void TF2BotDetectorSDLRenderer::SetFramerate(float newFrameTime)
{
	this->frameTime = newFrameTime;
}

float TF2BotDetectorSDLRenderer::GetFramerate() const
{
	return this->frameTime;
}

bool TF2BotDetectorSDLRenderer::InFocus() const
{
	// !ImGui::GetIO().AppFocusLost <- not implemented

	for (auto& viewport : ImGui::GetPlatformIO().Viewports) {
		if (ImGui::GetPlatformIO().Platform_GetWindowFocus(viewport)) {
			return true;
		}
	}

	return SDL_GetWindowFlags(window) & (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
}

std::string TF2BotDetectorSDLRenderer::RendererInfo() const
{
	const char* api = backend == TF2BDRenderBackend::D3D11 ? "Direct3D 11" : "OpenGL 4.3";
	return activeGpu.m_Name.empty() ? std::string(api) : fmt::format("{} on {}", api, activeGpu.m_Name);
}
