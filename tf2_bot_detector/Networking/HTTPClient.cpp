// cpp-httplib's CPPHTTPLIB_*_SUPPORT macros come from the httplib::httplib CMake target.

#include <mh/algorithm/multi_compare.hpp>
#include <mh/concurrency/thread_pool.hpp>
#include <mh/error/error_code_exception.hpp>
#include <mh/text/case_insensitive_string.hpp>

#include "GlobalDispatcher.h"
#include "HTTPClient.h"
#include "HTTPHelpers.h"

#pragma warning(push, 1)
#include <httplib.h>
#pragma warning(pop)

#include <array>
#include <filesystem>
#include <stdexcept>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace tf2_bot_detector;

namespace
{
	// Socket/TLS/timeout failure (no HTTP status received). Retried a few times, like cpprest's http_exception was.
	struct http_transport_error final : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

	// httplib::Result is move-only, but mh::task hands results back by copy, so the worker returns this instead.
	struct HTTPGetResult
	{
		bool m_Transferred = false;     // false: socket/TLS/timeout failure, see m_Error
		httplib::Error m_Error = httplib::Error::Success;
		int m_Status = 0;
		std::string m_Body;
	};

	// cpp-httplib is blocking; requests run here and the coroutine resumes when they finish.
	mh::thread_pool& GetHTTPThreadPool()
	{
		static mh::thread_pool s_Pool(4);
		return s_Pool;
	}

	void ConfigureClient(httplib::Client& client)
	{
		client.set_connection_timeout(std::chrono::seconds(15));
		client.set_read_timeout(std::chrono::seconds(30));
		client.set_write_timeout(std::chrono::seconds(15));
		client.set_follow_location(true); // cpprest (WinHTTP) followed redirects
		client.set_decompress(true);

#ifdef __linux__
		// vcpkg's static OpenSSL looks for CA certificates under its own install prefix, not the
		// distro's, so point it at the system bundle. (Windows uses the system cert store automatically.)
		static constexpr std::array CA_BUNDLES =
		{
			"/etc/ssl/certs/ca-certificates.crt",                 // Debian/Ubuntu/Arch/Gentoo
			"/etc/pki/tls/certs/ca-bundle.crt",                   // Fedora/RHEL
			"/etc/ssl/ca-bundle.pem",                             // openSUSE
			"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",  // CentOS/RHEL 7+
			"/etc/ssl/cert.pem",                                  // Alpine and others
		};
		for (const char* bundle : CA_BUNDLES)
		{
			std::error_code ec;
			if (std::filesystem::exists(bundle, ec))
			{
				client.set_ca_cert_path(bundle);
				break;
			}
		}
#endif
	}

	class HTTPClientImpl final : public IHTTPClient
	{
	public:
		std::string GetString(const URL& url) const override;
		mh::task<std::string> GetStringAsync(URL url) const override;

		RequestCounts GetRequestCounts() const override;

	private:
		mutable std::mutex m_InnerClientMutex;
		mutable std::map<std::string, std::shared_ptr<httplib::Client>> m_InnerClients;
		std::shared_ptr<httplib::Client> GetInnerClient(const URL& url) const;

		mutable std::atomic_uint32_t m_TotalRequestCount = 0;
		mutable std::atomic_uint32_t m_FailedRequestCount = 0;

		// This is a pretty stupid way of implementing this lol, but its easy
		struct RequestInProgressObj {};
		struct RequestQueuedObj {};
		const std::shared_ptr<RequestInProgressObj> m_InProgressRequestCount = std::make_shared<RequestInProgressObj>();
		const std::shared_ptr<RequestQueuedObj> m_QueuedRequestCount = std::make_shared<RequestQueuedObj>();
	};
}

std::string HTTPClientImpl::GetString(const URL& url) const
{
	auto task = GetStringAsync(url);
	task.wait();
	return std::move(task.get());
}

std::shared_ptr<httplib::Client> HTTPClientImpl::GetInnerClient(const URL& url) const
{
	std::lock_guard lock(m_InnerClientMutex);

	const std::string schemeHostPort = url.GetSchemeHostPort();
	if (auto found = m_InnerClients.find(schemeHostPort); found != m_InnerClients.end())
	{
		return found->second;
	}
	else
	{
		auto newClient = std::make_shared<httplib::Client>(schemeHostPort);
		ConfigureClient(*newClient);
		return m_InnerClients.emplace(schemeHostPort, newClient).first->second;
	}
}

static duration_t GetMinRequestInterval(const URL& url)
{
	if (url.m_Host.ends_with("akamaihd.net") ||
		url.m_Host.ends_with("steamstatic.com"))
	{
		return 0ms;
	}
	else if (url.m_Host == "api.steampowered.com")
	{
		if (mh::case_insensitive_view(url.m_Path).find("/GetPlayerItems/") != url.m_Path.npos)
			return 1000ms; // This is a slow/heavily throttled api

		return 100ms;
	}
	else if (url.m_Host == "steamcommunity.com")
	{
		return 2000ms;
	}

	return 500ms;
}

mh::task<std::string> HTTPClientImpl::GetStringAsync(URL url) const try
{
	auto self = shared_from_this(); // Make sure we don't vanish
	std::shared_ptr<RequestInProgressObj> inProgressObj;
	std::shared_ptr<RequestQueuedObj> queuedObj;

	const auto SetThrottled = [&](bool throttled)
	{
		if (throttled)
		{
			queuedObj = m_QueuedRequestCount;
			inProgressObj.reset();
		}
		else
		{
			inProgressObj = m_InProgressRequestCount;
			queuedObj.reset();
		}
	};

	SetThrottled(false);

	int32_t retryCount = 0;
	while (true)
	{
		using throttle_time_t = mh::thread_pool::clock_t::time_point;
		throttle_time_t throttleTime{};
		{
			const duration_t MIN_INTERVAL = GetMinRequestInterval(url);

			static std::mutex s_ThrottleMutex;
			static std::map<std::string, throttle_time_t> s_ThrottleDomains;
			std::lock_guard lock(s_ThrottleMutex);

			auto now = mh::thread_pool::clock_t::now();

			if (auto found = s_ThrottleDomains.find(url.m_Host);
				found != s_ThrottleDomains.end())
			{
				if (now > found->second)
				{
					found->second = now + MIN_INTERVAL;
				}
				else
				{
					throttleTime = found->second;
					found->second += MIN_INTERVAL;
				}
			}
			else
			{
				s_ThrottleDomains.emplace(url.m_Host, now + MIN_INTERVAL);
			}
		}

		if (throttleTime != throttle_time_t{})
		{
			SetThrottled(true);
			co_await GetDispatcher().co_delay_until(throttleTime);
			SetThrottled(false);
		}

		auto retryDelayTime = 10s;
		try
		{
			try // exceptions are fun and cool and not a code smell
			{
				auto requestIndex = ++m_TotalRequestCount;

				auto client = GetInnerClient(url);

				const auto startTime = tfbd_clock_t::now();

				const std::string path = url.m_Path.empty() ? "/"s : url.m_Path;
				HTTPGetResult result = co_await GetHTTPThreadPool().add_task([client, path]()
					{
						HTTPGetResult r;
						httplib::Result res = client->Get(path);
						r.m_Error = res.error();
						if (res)
						{
							r.m_Transferred = true;
							r.m_Status = res->status;
							r.m_Body = std::move(res->body);
						}
						return r;
					});

				if (!result.m_Transferred)
				{
					throw http_transport_error(fmt::format("Failed to HTTP GET {}: {}",
						url, httplib::to_string(result.m_Error)));
				}

				if (result.m_Status >= 400 && result.m_Status < 600)
					throw http_error((HTTPResponseCode)result.m_Status, fmt::format("Failed to HTTP GET {}", url));

				std::string stringResponse = std::move(result.m_Body); // already UTF-8

				const auto duration = tfbd_clock_t::now() - startTime;
				DebugLog("[{}ms] HTTP GET #{}: {}", std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(), requestIndex, url);

				co_return std::move(stringResponse);
			}
			catch (...)
			{
				++m_FailedRequestCount;
				throw;
			}
		}
		catch (const http_error& e)
		{
			const auto PrintRetryWarning = [&](MH_SOURCE_LOCATION_AUTO(location))
			{
				DebugLogWarning(location, "HTTP {} on {}, retrying...", (int)e.code().value(), url);
			};

			if (e.code() == HTTPResponseCode::TooManyRequests && retryCount < 10)
			{
				// retry a fair number of times for http 429, some stuff is aggressively throttled
				PrintRetryWarning();
			}
			else if (e.code() == HTTPResponseCode::InternalServerError && retryCount < 5)
			{
				// retry a few times for http 500-class errors, might be tf2bd-util being broken
				PrintRetryWarning();
			}
			else if (mh::any_eq(e.code(), HTTPResponseCode::BadGateway, HTTPResponseCode::ServiceUnavailable) && retryCount < 20)
			{
				// give a good like 20 tries, since they are likely indicitive of an api being temporarily down
				PrintRetryWarning();
			}
			else
			{
				throw; // give up
			}
		}
		catch (const http_transport_error&)
		{
			if (retryCount > 3)
			{
				// Give up after a few socket/timeout errors
				throw;
			}
		}

		// Wait and try again
		{
			SetThrottled(true);
			co_await GetDispatcher().co_delay_for(retryDelayTime);
			SetThrottled(false);
		}
		retryCount++;
		DebugLogWarning("Retry #{} for {}", retryCount, url);
	}
}
catch (const http_error&)
{
	DebugLogException("{}", url);
	throw;
}
catch (...)
{
	LogException("{}", url);
	throw;
}

auto HTTPClientImpl::GetRequestCounts() const -> RequestCounts
{
	return RequestCounts
	{
		.m_Total = m_TotalRequestCount,
		.m_Failed = m_FailedRequestCount,
		.m_InProgress = static_cast<uint32_t>(m_InProgressRequestCount.use_count() - 1),
		.m_Throttled = static_cast<uint32_t>(m_QueuedRequestCount.use_count() - 1),
	};
}

std::shared_ptr<IHTTPClient> tf2_bot_detector::IHTTPClient::Create()
{
	return std::make_shared<HTTPClientImpl>();
}
