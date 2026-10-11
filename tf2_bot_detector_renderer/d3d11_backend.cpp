#include "d3d11_backend.h"

#ifdef _WIN32

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>

#include <algorithm>
#include <iterator>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

namespace
{
	// Below this much dedicated VRAM we treat an adapter as integrated.
	constexpr uint64_t DISCRETE_VRAM_THRESHOLD = 512ull * 1024 * 1024;

	std::string WideToUTF8(const wchar_t* wide)
	{
		if (!wide || !*wide)
			return {};
		const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
		if (len <= 1)
			return {};
		std::string out(static_cast<size_t>(len - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), len, nullptr, nullptr);
		return out;
	}

	TF2BDGpuInfo MakeGpuInfo(const DXGI_ADAPTER_DESC1& desc)
	{
		TF2BDGpuInfo info;
		char id[16]{};
		std::snprintf(id, sizeof(id), "%04x:%04x", desc.VendorId, desc.DeviceId);
		info.m_ID = id;
		info.m_Name = WideToUTF8(desc.Description);
		info.m_DedicatedVRAM = desc.DedicatedVideoMemory;
		info.m_IsDiscrete = desc.DedicatedVideoMemory >= DISCRETE_VRAM_THRESHOLD;
		return info;
	}

	struct AdapterEntry
	{
		ComPtr<IDXGIAdapter1> m_Adapter;
		TF2BDGpuInfo m_Info;
	};

	/// <summary>
	/// hardware adapters, high-performance first (Win10 1803+), otherwise by VRAM.
	/// </summary>
	std::vector<AdapterEntry> EnumerateAdapters()
	{
		std::vector<AdapterEntry> adapters;

		ComPtr<IDXGIFactory1> factory1;
		if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory1))))
			return adapters;

		const auto addAdapter = [&](ComPtr<IDXGIAdapter1> adapter)
		{
			DXGI_ADAPTER_DESC1 desc{};
			if (FAILED(adapter->GetDesc1(&desc)))
				return;
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				return;

			auto info = MakeGpuInfo(desc);
			for (const auto& existing : adapters)
			{
				if (existing.m_Info.m_ID == info.m_ID && existing.m_Info.m_Name == info.m_Name)
					return;
			}
			adapters.push_back({ std::move(adapter), std::move(info) });
		};

		ComPtr<IDXGIFactory6> factory6;
		if (SUCCEEDED(factory1.As(&factory6)))
		{
			ComPtr<IDXGIAdapter1> adapter;
			for (UINT i = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(i,
				DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))); i++)
			{
				addAdapter(adapter);
				adapter.Reset();
			}
		}
		else
		{
			ComPtr<IDXGIAdapter1> adapter;
			for (UINT i = 0; factory1->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
			{
				addAdapter(adapter);
				adapter.Reset();
			}

			std::stable_sort(adapters.begin(), adapters.end(), [](const AdapterEntry& a, const AdapterEntry& b)
				{
					return a.m_Info.m_DedicatedVRAM > b.m_Info.m_DedicatedVRAM;
				});
		}

		return adapters;
	}
}

struct D3D11Backend::Impl
{
	ComPtr<ID3D11Device> m_Device;
	ComPtr<ID3D11DeviceContext> m_Context;
	ComPtr<IDXGISwapChain> m_SwapChain;
	ComPtr<ID3D11RenderTargetView> m_RenderTarget;
	bool m_ImGuiInitialized = false;

	bool CreateRenderTarget()
	{
		ComPtr<ID3D11Texture2D> backBuffer;
		if (FAILED(m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
			return false;
		return SUCCEEDED(m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_RenderTarget));
	}
};

D3D11Backend::D3D11Backend() : m_Impl(std::make_unique<Impl>()) {}
D3D11Backend::~D3D11Backend()
{
	Shutdown();
}

std::vector<TF2BDGpuInfo> D3D11Backend::EnumerateGpus()
{
	std::vector<TF2BDGpuInfo> gpus;
	for (auto& entry : EnumerateAdapters())
		gpus.push_back(std::move(entry.m_Info));
	return gpus;
}

bool D3D11Backend::Init(HWND__* hwnd, const std::string& preferredGpuID)
{
	auto adapters = EnumerateAdapters();

	// Pick: the user's choice if it's still present, otherwise the first (high performance) adapter.
	const AdapterEntry* chosen = nullptr;
	if (!preferredGpuID.empty())
	{
		for (const auto& entry : adapters)
		{
			if (entry.m_Info.m_ID == preferredGpuID)
			{
				chosen = &entry;
				break;
			}
		}
	}
	if (!chosen && !adapters.empty())
		chosen = &adapters.front();

	static constexpr D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	HRESULT hr = E_FAIL;
	if (chosen)
	{
		// When an explicit adapter is passed, the driver type must be UNKNOWN.
		hr = D3D11CreateDevice(chosen->m_Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
			featureLevels, static_cast<UINT>(std::size(featureLevels)), D3D11_SDK_VERSION,
			&m_Impl->m_Device, nullptr, &m_Impl->m_Context);
	}
	if (FAILED(hr))
	{
		// Let the driver decide.
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
			featureLevels, static_cast<UINT>(std::size(featureLevels)), D3D11_SDK_VERSION,
			&m_Impl->m_Device, nullptr, &m_Impl->m_Context);
		chosen = nullptr;
	}
	if (FAILED(hr))
		return false;

	// The swapchain has to come from the same factory as the device's adapter.
	ComPtr<IDXGIDevice> dxgiDevice;
	ComPtr<IDXGIAdapter> dxgiAdapter;
	ComPtr<IDXGIFactory> dxgiFactory;
	if (FAILED(m_Impl->m_Device.As(&dxgiDevice)) ||
		FAILED(dxgiDevice->GetAdapter(&dxgiAdapter)) ||
		FAILED(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))))
	{
		Shutdown();
		return false;
	}

	if (DXGI_ADAPTER_DESC desc{}; SUCCEEDED(dxgiAdapter->GetDesc(&desc)))
	{
		DXGI_ADAPTER_DESC1 desc1{};
		desc1.VendorId = desc.VendorId;
		desc1.DeviceId = desc.DeviceId;
		desc1.DedicatedVideoMemory = desc.DedicatedVideoMemory;
		std::copy(std::begin(desc.Description), std::end(desc.Description), std::begin(desc1.Description));
		m_ActiveGpu = MakeGpuInfo(desc1);
	}

	// Bitblt-model ("DISCARD") swapchain on purpose: the in-game overlay uses a color-keyed
	// layered window, which flip-model swapchains don't support.
	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferCount = 2;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = reinterpret_cast<HWND>(hwnd);
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	if (FAILED(dxgiFactory->CreateSwapChain(m_Impl->m_Device.Get(), &sd, &m_Impl->m_SwapChain)))
	{
		Shutdown();
		return false;
	}
	dxgiFactory->MakeWindowAssociation(reinterpret_cast<HWND>(hwnd), DXGI_MWA_NO_ALT_ENTER);

	if (!m_Impl->CreateRenderTarget())
	{
		Shutdown();
		return false;
	}

	return true;
}

void D3D11Backend::Shutdown()
{
	if (!m_Impl)
		return;

	if (m_Impl->m_ImGuiInitialized)
	{
		ImGui_ImplDX11_Shutdown();
		m_Impl->m_ImGuiInitialized = false;
	}

	m_Impl->m_RenderTarget.Reset();
	m_Impl->m_SwapChain.Reset();
	m_Impl->m_Context.Reset();
	m_Impl->m_Device.Reset();
}

void D3D11Backend::NewFrame()
{
	if (!m_Impl->m_ImGuiInitialized)
	{
		ImGui_ImplDX11_Init(m_Impl->m_Device.Get(), m_Impl->m_Context.Get());
		m_Impl->m_ImGuiInitialized = true;
	}

	ImGui_ImplDX11_NewFrame();
}

void D3D11Backend::Render(ImDrawData* drawData, const float clearColor[4])
{
	ID3D11RenderTargetView* rtv = m_Impl->m_RenderTarget.Get();
	m_Impl->m_Context->OMSetRenderTargets(1, &rtv, nullptr);
	m_Impl->m_Context->ClearRenderTargetView(rtv, clearColor);
	ImGui_ImplDX11_RenderDrawData(drawData);
}

bool D3D11Backend::Present(bool vsync)
{
	const HRESULT hr = m_Impl->m_SwapChain->Present(vsync ? 1 : 0, 0);
	return hr != DXGI_STATUS_OCCLUDED;
}

void D3D11Backend::OnResize()
{
	if (!m_Impl->m_SwapChain)
		return;

	m_Impl->m_RenderTarget.Reset();
	m_Impl->m_SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
	m_Impl->CreateRenderTarget();
}

void* D3D11Backend::CreateTextureRGBA(const void* rgba, uint32_t width, uint32_t height)
{
	if (!m_Impl->m_Device || !rgba || width == 0 || height == 0)
		return nullptr;

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = rgba;
	data.SysMemPitch = width * 4;

	ComPtr<ID3D11Texture2D> texture;
	if (FAILED(m_Impl->m_Device->CreateTexture2D(&desc, &data, &texture)))
		return nullptr;

	ID3D11ShaderResourceView* srv = nullptr;
	if (FAILED(m_Impl->m_Device->CreateShaderResourceView(texture.Get(), nullptr, &srv)))
		return nullptr;

	return srv; // the view keeps the texture alive
}

void D3D11Backend::DestroyTexture(void* texture)
{
	if (texture)
		static_cast<ID3D11ShaderResourceView*>(texture)->Release();
}

#endif
