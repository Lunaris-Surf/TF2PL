#pragma once

#include "Clock.h"
#include "SteamID.h"

#include <nlohmann/json.hpp>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace httplib
{
	class Server;
}

namespace tf2_bot_detector
{
	class PlayerListExporter;
	class TF2BDApplication;

	/// <summary>
	/// Optional browser front end (Settings > UI > Interface = Web).
	///
	/// Serves an embedded HTML/JS page on 127.0.0.1 only. The page gets live updates over Server-Sent Events
	/// (falling back to polling), and posts actions for everything the native UI can do except the overlay and
	/// renderer/GPU settings. All application state is read and changed on the app's update thread: the HTTP
	/// threads only hand out the latest JSON strings and queue actions, which Update() validates and runs.
	///
	/// Protection: loopback bind; Host header must be 127.0.0.1/localhost/local.tf2pl on our port (blocks DNS rebinding);
	/// every /api request needs a per-run random token that is only embedded in the page we serve (header, or a
	/// query parameter for EventSource/img which can't send headers), and no CORS headers are sent, so other
	/// sites can neither read the token nor send it.
	/// </summary>
	class WebUIController final
	{
	public:
		explicit WebUIController(TF2BDApplication& app);
		~WebUIController();

		WebUIController(const WebUIController&) = delete;
		WebUIController& operator=(const WebUIController&) = delete;

		/// Call from the app update thread. Starts/restarts the server as needed, refreshes the data the page
		/// sees and runs queued actions.
		void Update();

		bool IsRunning() const;
		std::string GetURL() const;
		std::string GetError() const;
		uint16_t GetPort() const { return m_Port; }
		void OpenInBrowser() const;
		size_t GetLiveClientCount() const { return m_LiveClients; }

	private:
		bool Start(uint16_t port);
		void Stop();

		void Refresh(bool force);
		void RunQueuedActions();
		void RunAction(const nlohmann::json& action);

		TF2BDApplication& m_Application;

		std::unique_ptr<httplib::Server> m_Server;
		std::thread m_Thread;
		std::shared_ptr<std::atomic_bool> m_ThreadExited; // set by the server thread if listen() returns
		uint16_t m_Port = 0;
		std::string m_Token;
		bool m_BrowserOpened = false;
		bool m_HostAliasAttempted = false;
		bool m_HostAliasReady = false;
		std::future<bool> m_HostAliasSetup;

		/// one stream of data the page subscribes to; rebuilt on the update thread, sent when it changes
		enum Channel : size_t
		{
			Channel_State,
			Channel_Chat,
			Channel_Log,
			Channel_Settings,
			Channel_Dossier,
			Channel_Lists,

			Channel_Count,
		};
		static const char* ChannelName(size_t channel);

		struct Published
		{
			std::string m_Data = "null";
			uint64_t m_Version = 0;
		};

		mutable std::mutex m_Mutex;      // guards everything below
		std::condition_variable m_Changed;
		std::array<Published, Channel_Count> m_Channels;
		std::deque<std::string> m_Actions;
		std::string m_Error;
		std::string m_StreamerAvatarPath; // read by the HTTP threads for /api/avatar
		bool m_Stopping = false;
		std::atomic<size_t> m_LiveClients = 0;

		void Publish(Channel channel, std::string data);

		// update-thread state
		time_point_t m_LastFastRefresh{};
		time_point_t m_LastSlowRefresh{};
		time_point_t m_LastStartAttempt{};
		bool m_ForceRefresh = true;
		std::optional<SteamID> m_FocusedPlayer;      // dossier subject
		bool m_FocusedSelf = false;                  // dossier subject is you (shown as "self" in streamer mode)
		std::optional<nlohmann::json> m_ListQuery;   // Player Lists browser query (empty = closed)
		size_t m_ListQueryPlayerCount = size_t(-1);
		bool m_ListsDirty = false;
		std::unique_ptr<PlayerListExporter> m_Exporter;
	};
}
