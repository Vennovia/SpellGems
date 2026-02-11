// Configuration types and accessors for SpellGems.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace SpellGems
{
	enum class SpellTier : std::uint8_t
	{
		Novice = 0,
		Apprentice,
		Adept,
		Expert,
		Master,
		Total
	};

	struct TierSettings
	{
		float cooldown;
		std::int32_t uses;
	};

	class Config
	{
	public:
		static Config& GetSingleton();

		void Load();
		void Save() const;

		const TierSettings& GetTierSettings(SpellTier tier) const;
		TierSettings& GetTierSettings(SpellTier tier);

		std::uint32_t GetStoreKey() const;
		void SetStoreKey(std::uint32_t key);
		std::uint32_t GetActivationKey(std::size_t index) const;
		void SetActivationKey(std::size_t index, std::uint32_t key);
		std::uint8_t GetMaxStoredGems() const;
		void SetMaxStoredGems(std::uint8_t value);

		bool IsFiniteUse() const;
		void SetFiniteUse(bool value);

		bool ShowUsesRemaining() const;
		void SetShowUsesRemaining(bool value);
		bool RequireFilledSoulGem() const;
		void SetRequireFilledSoulGem(bool value);
		bool AllowAnyGemTier() const;
		void SetAllowAnyGemTier(bool value);
		bool BlackSoulGemBoosts() const;
		void SetBlackSoulGemBoosts(bool value);
		bool NormalGemPenalty() const;
		void SetNormalGemPenalty(bool value);
		bool AzurasStarBoost() const;
		void SetAzurasStarBoost(bool value);
		float GetFocusSpellDuration() const;
		void SetFocusSpellDuration(float value);
		float GetStarCooldown() const;
		void SetStarCooldown(float value);
		std::uint32_t GetFragmentFormId() const;
		void SetFragmentFormId(std::uint32_t value);
		std::uint32_t GetFragmentCount(SpellTier tier) const;
		void SetFragmentCount(SpellTier tier, std::uint32_t value);

		static std::string_view GetTierName(SpellTier tier);

	private:
		Config();

		std::array<TierSettings, static_cast<std::size_t>(SpellTier::Total)> tierSettings_{};
		std::uint32_t storeKey_{};
		std::array<std::uint32_t, 5> activationKeys_{};
		std::uint8_t maxStoredGems_{ 3 };
		bool finiteUse_{ true };
		bool showUsesRemaining_{ true };
		bool requireFilledSoulGem_{ true };
		bool allowAnyGemTier_{ false };
		bool blackSoulGemBoosts_{ true };
		bool normalGemPenalty_{ true };
		bool azurasStarBoost_{ true };
		float focusSpellDuration_{ 2.0f };
		float starCooldown_{ 3.0f };
		std::uint32_t fragmentFormId_{ 0x00067181 };
		std::array<std::uint32_t, static_cast<std::size_t>(SpellTier::Total)> fragmentCounts_{};
	};
}
