#include "TextureManager.h"
#include "Bitmap.h"

#include "ITF2BotDetectorRenderer.h"

#include <mh/concurrency/thread_sentinel.hpp>

#include <cassert>
#include <cstddef>
#include <vector>

using namespace tf2_bot_detector;

// Textures are created through the active renderer (Direct3D 11 or OpenGL),
// so nothing here talks to a graphics API directly.

namespace
{
	class TextureManager;

	/// <summary>
	/// expand 1/2/3/4 channel 8-bit pixels to RGBA8, which both backends accept.
	/// 1 channel = grey, 2 channels = grey + alpha (matches the old GL swizzles).
	/// </summary>
	std::vector<uint8_t> ToRGBA8(const Bitmap& bitmap)
	{
		const size_t pixelCount = size_t(bitmap.GetWidth()) * bitmap.GetHeight();
		const auto* src = static_cast<const uint8_t*>(bitmap.GetData());
		const uint8_t channels = bitmap.GetChannelCount();

		std::vector<uint8_t> rgba(pixelCount * 4);
		for (size_t i = 0; i < pixelCount; i++)
		{
			uint8_t* dst = &rgba[i * 4];
			const uint8_t* px = &src[i * channels];
			switch (channels)
			{
			case 1: dst[0] = dst[1] = dst[2] = px[0]; dst[3] = 255; break;
			case 2: dst[0] = dst[1] = dst[2] = px[0]; dst[3] = px[1]; break;
			case 3: dst[0] = px[0]; dst[1] = px[1]; dst[2] = px[2]; dst[3] = 255; break;
			default: dst[0] = px[0]; dst[1] = px[1]; dst[2] = px[2]; dst[3] = px[3]; break;
			}
		}

		return rgba;
	}

	class Texture final : public ITexture
	{
	public:
		Texture(const Bitmap& bitmap, const TextureSettings& settings);
		~Texture() override;

		handle_type GetHandle() const override { return reinterpret_cast<handle_type>(m_Handle); }
		const TextureSettings& GetSettings() const override { return m_Settings; }

		uint16_t GetWidth() const override { return m_Width; }
		uint16_t GetHeight() const override { return m_Height; }

	private:
		TF2BotDetectorRendererBase* m_Renderer = nullptr;
		void* m_Handle = nullptr;
		TextureSettings m_Settings{};
		uint16_t m_Width{};
		uint16_t m_Height{};
	};

	class TextureManager final : public ITextureManager
	{
	public:
		void EndFrame() override;
		std::shared_ptr<ITexture> CreateTexture(const Bitmap& bitmap, const TextureSettings& settings) override;
		size_t GetActiveTextureCount() const override { return m_Textures.size(); }

	private:
		std::vector<std::shared_ptr<Texture>> m_Textures;
		mh::thread_sentinel m_Sentinel;
	};
}

std::shared_ptr<ITextureManager> tf2_bot_detector::ITextureManager::Create()
{
	return std::make_unique<TextureManager>();
}

void TextureManager::EndFrame()
{
	m_Sentinel.check();
	std::erase_if(m_Textures, [](const std::shared_ptr<Texture>& t)
		{
			return t.use_count() == 1;
		});
}

std::shared_ptr<ITexture> TextureManager::CreateTexture(const Bitmap& bitmap, const TextureSettings& settings)
{
	m_Sentinel.check();
	return m_Textures.emplace_back(std::make_shared<Texture>(bitmap, settings));
}

Texture::Texture(const Bitmap& bitmap, const TextureSettings& settings) :
	m_Renderer(TF2BotDetectorRendererBase::GetRenderer()),
	m_Settings(settings),
	m_Width(static_cast<uint16_t>(bitmap.GetWidth())),
	m_Height(static_cast<uint16_t>(bitmap.GetHeight()))
{
	assert(m_Renderer);
	if (!m_Renderer || bitmap.empty())
		return;

	// mipmaps were already disabled in the GL path; m_EnableMips is kept for API compatibility.
	const auto rgba = ToRGBA8(bitmap);
	m_Handle = m_Renderer->CreateTextureRGBA(rgba.data(), bitmap.GetWidth(), bitmap.GetHeight());
}

Texture::~Texture()
{
	if (m_Renderer && m_Handle)
		m_Renderer->DestroyTexture(m_Handle);
}
