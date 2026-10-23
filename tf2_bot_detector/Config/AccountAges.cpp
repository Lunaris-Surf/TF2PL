#include "AccountAges.h"
#include "SteamID.h"
#include "Util/JSONUtils.h"
#include "Application.h"
#include "DB/TempDB.h"

#include <tf2pl/concurrency/thread_sentinel.hpp>
#include <tf2pl/error/not_implemented_error.hpp>
#include <tf2pl/math/interpolation.hpp>
#include <tf2pl/source_location.hpp>
#include <nlohmann/json.hpp>

#include <cassert>

using namespace tf2_bot_detector;

namespace tf2_bot_detector::DB
{
	class ITempDB;
}

namespace
{
	/// Estimates an account's steam account age, by given steamid.
	/// TODO: move this to Util/, because this has nothing to do with config.
	class AccountAges final : public IAccountAges
	{
	public:
		void OnDataReady(const SteamID& id, time_point_t creationTime) override;

		std::optional<time_point_t> EstimateAccountCreationTime(const SteamID& id) const override;

	private:
		[[nodiscard]] bool CheckSteamIDValid(const SteamID& id, TF2PL_SOURCE_LOCATION_AUTO(location)) const;
	};
}

std::shared_ptr<IAccountAges> tf2_bot_detector::IAccountAges::Create()
{
	return std::make_shared<AccountAges>();
}

bool AccountAges::CheckSteamIDValid(const SteamID& id, const tf2pl::source_location& location) const
{
	if (id.Type != SteamAccountType::Individual)
	{
		LogError(location, "Steam ID ({}) must be for an individual account", id);
		return false;
	}

	return true;
}

void AccountAges::OnDataReady(const SteamID& id, time_point_t creationTime)
{
	if (!CheckSteamIDValid(id))
		return;

	DB::ITempDB& tempDB = TF2BDApplication::GetApplication().GetTempDB();
	DB::AccountAgeInfo info{};
	info.m_SteamID = id;
	info.m_CreationTime = creationTime;
	tempDB.Store(info);
}

std::optional<time_point_t> AccountAges::EstimateAccountCreationTime(const SteamID& id) const
{
	if (!CheckSteamIDValid(id))
		return std::nullopt;

	std::optional<DB::AccountAgeInfo> lower, upper;
	TF2BDApplication::GetApplication().GetTempDB().GetNearestAccountAgeInfos(id, lower, upper);

	if (!lower.has_value())
		return std::nullopt;   // super new, we don't have any data for this
	if (!upper.has_value())
		return lower.value().m_CreationTime;  // Nothing to interpolate to, pick the lower value

	if (lower->m_CreationTime == upper->m_CreationTime)
		return lower->m_CreationTime;  // they're the same picture

	// Interpolate the time between the nearest lower and upper steam ID
	const auto interpValue = tf2pl::remap(id.GetAccountID(),
		lower->m_SteamID.GetAccountID(), upper->m_SteamID.GetAccountID(),
		lower->m_CreationTime.time_since_epoch().count(), upper->m_CreationTime.time_since_epoch().count());

	assert(interpValue >= 0);

	return time_point_t(time_point_t::duration(interpValue));
}
