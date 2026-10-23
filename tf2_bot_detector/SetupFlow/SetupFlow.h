#pragma once

#include "ISetupFlowPage.h"

#include <memory>
#include <vector>

namespace tf2_bot_detector
{
	class Settings;

	class SetupFlow final
	{
	public:
		SetupFlow();

		// Returns true if the setup flow needs to draw.
		[[nodiscard]] bool OnUpdate(const Settings& settings);
		[[nodiscard]] bool OnDraw(Settings& settings, const ISetupFlowPage::DrawState& ds);

		bool ShouldDraw() const { return m_ShouldDraw; }

		SetupFlowPage GetCurrentPage() const;

		/// <summary>
		/// Preview mode: while set, pages that only wait for Steam/TF2 (Check Steam open, TF2 launch, chat
		/// wrapper verification) are skipped so the main UI can run without the game. Other setup pages
		/// still show.
		/// </summary>
		void SetPreviewBypass(bool bypass);
		bool IsPreviewBypassed() const { return m_PreviewBypass; }

		/// true if every page that wants to open is one preview mode can skip (see SetPreviewBypass)
		bool CanPreviewBypass(const Settings& settings) const;

	private:
		bool m_ShouldDraw = false;
		bool m_PreviewBypass = false;
		static bool IsPreviewBypassablePage(SetupFlowPage page);
		std::vector<std::unique_ptr<ISetupFlowPage>> m_Pages;

		void GetPageState(const Settings& settings, size_t& currentPage, bool& hasNextPage) const;

		static constexpr size_t INVALID_PAGE = size_t(-1);
		size_t m_ActivePage = INVALID_PAGE;
	};
}
