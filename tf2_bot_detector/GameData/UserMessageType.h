#pragma once

#include <tf2pl/reflection/enum.hpp>

namespace tf2_bot_detector
{
	enum class UserMessageType
	{
		// From "meta game" on 7/18/2020
		Geiger = 0,
		Train = 1,
		HudText = 2,
		SayText = 3,
		SayText2 = 4,
		TextMsg = 5,
		ResetHUD = 6,
		GameTitle = 7,
		ItemPickup = 8,
		ShowMenu = 9,
		Shake = 10,
		Fade = 11,
		VGUIMenu = 12,
		Rumble = 13,
		CloseCaption = 14,
		SendAudio = 15,
		VoiceMask = 16,
		RequestState = 17,
		Damage = 18,
		HintText = 19,
		KeyHintText = 20,
		HudMsg = 21,
		AmmoDenied = 22,
		AchievementEvent = 23,
		UpdateRadar = 24,
		VoiceSubtitle = 25,
		HudNotify = 26,
		HudNotifyCustom = 27,
		PlayerStatsUpdate = 28,
		MapStatsUpdate = 29,
		PlayerIgnited = 30,
		PlayerIgnitedInv = 31,
		HudArenaNotify = 32,
		UpdateAchievement = 33,
		TrainingMsg = 34,
		TrainingObjective = 35,
		DamageDodged = 36,
		PlayerJarated = 37,
		PlayerExtinguished = 38,
		PlayerJaratedFade = 39,
		PlayerShieldBlocked = 40,
		BreakModel = 41,
		CheapBreakModel = 42,
		BreakModel_Pumpkin = 43,
		BreakModelRocketDud = 44,
		CallVoteFailed = 45,
		VoteStart = 46,
		VotePass = 47,
		VoteFailed = 48,
		VoteSetup = 49,
		PlayerBonusPoints = 50,
		RDTeamPointsChanged = 51,
		SpawnFlyingBird = 52,
		PlayerGodRayEffect = 53,
		PlayerTeleportHomeEffect = 54,
		MVMStatsReset = 55,
		MVMPlayerEvent = 56,
		MVMResetPlayerStats = 57,
		MVMWaveFailed = 58,
		MVMAnnouncement = 59,
		MVMPlayerUpgradedEvent = 60,
		MVMVictory = 61,
		MVMWaveChange = 62,
		MVMLocalPlayerUpgradesClear = 63,
		MVMLocalPlayerUpgradesValue = 64,
		MVMLocalPlayerWaveSpendingStats = 65,
		MVMLocalPlayerWaveSpendingValue = 66,
		MVMLocalPlayerUpgradeSpending = 67,
		MVMServerKickTimeUpdate = 68,
		PlayerLoadoutUpdated = 69,
		PlayerTauntSoundLoopStart = 70,
		PlayerTauntSoundLoopEnd = 71,
		ForcePlayerViewAngles = 72,
		BonusDucks = 73,
		EOTLDuckEvent = 74,
		PlayerPickupWeapon = 75,
		QuestObjectiveCompleted = 76,
		SPHapWeapEvent = 77,
		HapDmg = 78,
		HapPunch = 79,
		HapSetDrag = 80,
		HapSetConst = 81,
		HapMeleeContact = 82,
	};
}

TF2PL_ENUM_REFLECT_BEGIN(tf2_bot_detector::UserMessageType)
	TF2PL_ENUM_REFLECT_VALUE(Geiger)
	TF2PL_ENUM_REFLECT_VALUE(Train)
	TF2PL_ENUM_REFLECT_VALUE(HudText)
	TF2PL_ENUM_REFLECT_VALUE(SayText)
	TF2PL_ENUM_REFLECT_VALUE(SayText2)
	TF2PL_ENUM_REFLECT_VALUE(TextMsg)
	TF2PL_ENUM_REFLECT_VALUE(ResetHUD)
	TF2PL_ENUM_REFLECT_VALUE(GameTitle)
	TF2PL_ENUM_REFLECT_VALUE(ItemPickup)
	TF2PL_ENUM_REFLECT_VALUE(ShowMenu)
	TF2PL_ENUM_REFLECT_VALUE(Shake)
	TF2PL_ENUM_REFLECT_VALUE(Fade)
	TF2PL_ENUM_REFLECT_VALUE(VGUIMenu)
	TF2PL_ENUM_REFLECT_VALUE(Rumble)
	TF2PL_ENUM_REFLECT_VALUE(CloseCaption)
	TF2PL_ENUM_REFLECT_VALUE(SendAudio)
	TF2PL_ENUM_REFLECT_VALUE(VoiceMask)
	TF2PL_ENUM_REFLECT_VALUE(RequestState)
	TF2PL_ENUM_REFLECT_VALUE(Damage)
	TF2PL_ENUM_REFLECT_VALUE(HintText)
	TF2PL_ENUM_REFLECT_VALUE(KeyHintText)
	TF2PL_ENUM_REFLECT_VALUE(HudMsg)
	TF2PL_ENUM_REFLECT_VALUE(AmmoDenied)
	TF2PL_ENUM_REFLECT_VALUE(AchievementEvent)
	TF2PL_ENUM_REFLECT_VALUE(UpdateRadar)
	TF2PL_ENUM_REFLECT_VALUE(VoiceSubtitle)
	TF2PL_ENUM_REFLECT_VALUE(HudNotify)
	TF2PL_ENUM_REFLECT_VALUE(HudNotifyCustom)
	TF2PL_ENUM_REFLECT_VALUE(PlayerStatsUpdate)
	TF2PL_ENUM_REFLECT_VALUE(MapStatsUpdate)
	TF2PL_ENUM_REFLECT_VALUE(PlayerIgnited)
	TF2PL_ENUM_REFLECT_VALUE(PlayerIgnitedInv)
	TF2PL_ENUM_REFLECT_VALUE(HudArenaNotify)
	TF2PL_ENUM_REFLECT_VALUE(UpdateAchievement)
	TF2PL_ENUM_REFLECT_VALUE(TrainingMsg)
	TF2PL_ENUM_REFLECT_VALUE(TrainingObjective)
	TF2PL_ENUM_REFLECT_VALUE(DamageDodged)
	TF2PL_ENUM_REFLECT_VALUE(PlayerJarated)
	TF2PL_ENUM_REFLECT_VALUE(PlayerExtinguished)
	TF2PL_ENUM_REFLECT_VALUE(PlayerJaratedFade)
	TF2PL_ENUM_REFLECT_VALUE(PlayerShieldBlocked)
	TF2PL_ENUM_REFLECT_VALUE(BreakModel)
	TF2PL_ENUM_REFLECT_VALUE(CheapBreakModel)
	TF2PL_ENUM_REFLECT_VALUE(BreakModel_Pumpkin)
	TF2PL_ENUM_REFLECT_VALUE(BreakModelRocketDud)
	TF2PL_ENUM_REFLECT_VALUE(CallVoteFailed)
	TF2PL_ENUM_REFLECT_VALUE(VoteStart)
	TF2PL_ENUM_REFLECT_VALUE(VotePass)
	TF2PL_ENUM_REFLECT_VALUE(VoteFailed)
	TF2PL_ENUM_REFLECT_VALUE(VoteSetup)
	TF2PL_ENUM_REFLECT_VALUE(PlayerBonusPoints)
	TF2PL_ENUM_REFLECT_VALUE(RDTeamPointsChanged)
	TF2PL_ENUM_REFLECT_VALUE(SpawnFlyingBird)
	TF2PL_ENUM_REFLECT_VALUE(PlayerGodRayEffect)
	TF2PL_ENUM_REFLECT_VALUE(PlayerTeleportHomeEffect)
	TF2PL_ENUM_REFLECT_VALUE(MVMStatsReset)
	TF2PL_ENUM_REFLECT_VALUE(MVMPlayerEvent)
	TF2PL_ENUM_REFLECT_VALUE(MVMResetPlayerStats)
	TF2PL_ENUM_REFLECT_VALUE(MVMWaveFailed)
	TF2PL_ENUM_REFLECT_VALUE(MVMAnnouncement)
	TF2PL_ENUM_REFLECT_VALUE(MVMPlayerUpgradedEvent)
	TF2PL_ENUM_REFLECT_VALUE(MVMVictory)
	TF2PL_ENUM_REFLECT_VALUE(MVMWaveChange)
	TF2PL_ENUM_REFLECT_VALUE(MVMLocalPlayerUpgradesClear)
	TF2PL_ENUM_REFLECT_VALUE(MVMLocalPlayerUpgradesValue)
	TF2PL_ENUM_REFLECT_VALUE(MVMLocalPlayerWaveSpendingStats)
	TF2PL_ENUM_REFLECT_VALUE(MVMLocalPlayerWaveSpendingValue)
	TF2PL_ENUM_REFLECT_VALUE(MVMLocalPlayerUpgradeSpending)
	TF2PL_ENUM_REFLECT_VALUE(MVMServerKickTimeUpdate)
	TF2PL_ENUM_REFLECT_VALUE(PlayerLoadoutUpdated)
	TF2PL_ENUM_REFLECT_VALUE(PlayerTauntSoundLoopStart)
	TF2PL_ENUM_REFLECT_VALUE(PlayerTauntSoundLoopEnd)
	TF2PL_ENUM_REFLECT_VALUE(ForcePlayerViewAngles)
	TF2PL_ENUM_REFLECT_VALUE(BonusDucks)
	TF2PL_ENUM_REFLECT_VALUE(EOTLDuckEvent)
	TF2PL_ENUM_REFLECT_VALUE(PlayerPickupWeapon)
	TF2PL_ENUM_REFLECT_VALUE(QuestObjectiveCompleted)
	TF2PL_ENUM_REFLECT_VALUE(SPHapWeapEvent)
	TF2PL_ENUM_REFLECT_VALUE(HapDmg)
	TF2PL_ENUM_REFLECT_VALUE(HapPunch)
	TF2PL_ENUM_REFLECT_VALUE(HapSetDrag)
	TF2PL_ENUM_REFLECT_VALUE(HapSetConst)
	TF2PL_ENUM_REFLECT_VALUE(HapMeleeContact)
TF2PL_ENUM_REFLECT_END()
