#pragma once

#include "Clock.h"

#include <nlohmann/json_fwd.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace httplib
{
	class Server;
}

namespace tf2_bot_detector
{
	class TF2BDApplication;

	/// <summary>
	/// Optional browser front end (Settings > UI > Interface = Web).
	///
	/// Serves an embedded HTML/JS page on 127.0.0.1 only. The page polls a JSON snapshot of the scoreboard and
	/// chat, and posts actions (mark/unmark, custom tags, votekick, switch back to native). All application state
	/// is read and changed on the app's update thread: the HTTP thread only hands out the latest snapshot string and
	/// queues actions, which Update() validates and runs.
	///
	/// Protection: loopback bind; Host header must be 127.0.0.1/localhost on our port (blocks DNS rebinding);
	/// every /api request needs a per-run random token that is only embedded in the page we serve, and no CORS
	/// headers are sent, so other sites can neither read the token nor send it.
	/// </summary>
	class WebUIController final
	{
	public:
		explicit WebUIController(TF2BDApplication& app);
		~WebUIController();

		WebUIController(const WebUIController&) = delete;
		WebUIController& operator=(const WebUIController&) = delete;

		/// Call from the app update thread. Starts/restarts the server as needed, refreshes the snapshot and
		/// runs queued actions.
		void Update();

		bool IsRunning() const;
		std::string GetURL() const;
		std::string GetError() const;
		uint16_t GetPort() const { return m_Port; }
		void OpenInBrowser() const;

	private:
		bool Start(uint16_t port);
		void Stop();

		std::string BuildSnapshot();
		void RunQueuedActions();
		void RunAction(const nlohmann::json& action);

		TF2BDApplication& m_Application;

		std::unique_ptr<httplib::Server> m_Server;
		std::thread m_Thread;
		std::shared_ptr<std::atomic_bool> m_ThreadExited; // set by the server thread if listen() returns
		uint16_t m_Port = 0;
		std::string m_Token;
		bool m_BrowserOpened = false;

		mutable std::mutex m_Mutex;      // guards everything below
		std::string m_Snapshot = "{}";
		std::deque<std::string> m_Actions;
		std::string m_Error;
		uint64_t m_SnapshotVersion = 0;

		time_point_t m_LastSnapshot{};
		time_point_t m_LastStartAttempt{};
	};
}
