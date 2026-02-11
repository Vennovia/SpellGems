#pragma once

#include "SpellGems/Config.h"
#include "SpellGems/Serialization.h"

#include <atomic>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "RE/B/BSTEvent.h"
#include "RE/E/ExtraDataList.h"
#include "RE/I/InventoryEntryData.h"
#include "RE/S/SpellItem.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESSoulGem.h"
#include "RE/T/TESContainerChangedEvent.h"

#include <keyhandler/keyhandler.h>

// Manager for storing, activating, and tracking spell gems.
namespace SpellGems
{
	class SpellGemManager
	{
	public:
		static SpellGemManager& GetSingleton();

		void TryStoreSelectedSpell();
		void RegisterUseEventSink();
		void RegisterActivationKeys();
		void ActivateStoredGemSlot(std::size_t index);
		bool ResolveStoredGemSpell(RE::TESForm* form, GemKey& key, StoredSpellData& data, RE::SpellItem*& spell) const;
		void ConsumeStoredGemUse(RE::TESSoulGem& baseGem, const GemKey& key, const StoredSpellData& data);

	private:
		SpellGemManager() = default;

		struct SelectedGem
		{
			RE::InventoryEntryData* entry;
			RE::ExtraDataList* extraList;
		};

		class StoredGemUseEventSink : public RE::BSTEventSink<RE::TESContainerChangedEvent>
		{
		public:
			explicit StoredGemUseEventSink(SpellGemManager& manager) : manager_(manager) {}
			RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* event,
				RE::BSTEventSource<RE::TESContainerChangedEvent>*) override;

		private:
			SpellGemManager& manager_;
		};

		struct StoredGemFormKey
		{
			RE::FormID baseId;
			RE::FormID spellId;
			SpellTier tier;
			std::int32_t usesRemaining;

			bool operator==(const StoredGemFormKey& other) const
			{
				return baseId == other.baseId && spellId == other.spellId && tier == other.tier && usesRemaining == other.usesRemaining;
			}
		};

		struct StoredGemFormKeyHash
		{
			std::size_t operator()(const StoredGemFormKey& key) const
			{
				std::size_t seed = std::hash<RE::FormID>{}(key.baseId);
				seed ^= std::hash<RE::FormID>{}(key.spellId) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
				seed ^= std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(key.tier)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
				seed ^= std::hash<std::int32_t>{}(key.usesRemaining) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
				return seed;
			}
		};

		SelectedGem GetSelectedSoulGem() const;
		RE::SpellItem* GetRightHandSpell() const;
		bool TryGetSpellTier(const RE::SpellItem& spell, SpellTier& tier) const;
		SpellTier GetSpellTier(const RE::SpellItem& spell) const;
		SpellTier GetGemTier(const RE::TESSoulGem& gem) const;
		RE::TESSoulGem* GetOrCreateStoredGemForm(RE::TESSoulGem& baseGem, const RE::SpellItem& spell, SpellTier tier, std::int32_t usesRemaining);
		std::uint16_t GetOrCreateUniqueId(const RE::TESSoulGem& gem, RE::ExtraDataList& extraList) const;
		RE::ExtraDataList* CreateExtraDataList() const;
		std::string BuildDisplayName(const RE::SpellItem& spell, SpellTier tier) const;
		RE::BSEventNotifyControl HandleContainerChanged(const RE::TESContainerChangedEvent& event);
		void CastStoredSpell(RE::SpellItem& spell, RE::PlayerCharacter& player, bool isBlackSoulGem, bool isReusableStar, bool isAzurasStar);
		void StopFocusSpellCast(std::size_t index);
		void GrantFragmentsToPlayer(SpellTier tier) const;
		bool IsReusableStar(RE::FormID formId) const;
		bool IsAzurasStar(RE::FormID formId) const;
		bool IsBlackSoulGem(const RE::TESSoulGem& gem) const;
		void RefreshStoredGemSlots();

		void LogMessage(const std::string& message) const;

		StoredGemUseEventSink useEventSink_{ *this };
		std::unordered_map<StoredGemFormKey, RE::TESSoulGem*, StoredGemFormKeyHash> storedGemForms_;
		std::vector<GemKey> storedGemSlots_;
		std::vector<KeyHandlerEvent> activationHandles_;
		std::vector<KeyHandlerEvent> activationReleaseHandles_;
		std::optional<std::size_t> activeFocusSlot_{};
		std::atomic<std::uint64_t> focusCastId_{ 0 };
		std::optional<RE::MagicSystem::CastingSource> focusCasterSource_{};
		float focusPreviousCost_{ 0.0f };
		bool focusCostActive_{ false };
		float focusStartMagicka_{ 0.0f };
	};
}
