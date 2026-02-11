// Serialization types and persistence for stored spell data.
#pragma once

#include "SpellGems/Config.h"

#include <cstdint>
#include <unordered_map>

#include "RE/F/FormTypes.h"
#include "SKSE/Interfaces.h"

namespace SpellGems
{
	struct GemKey
	{
		RE::FormID baseId{};
		std::uint16_t uniqueId{};

		friend bool operator==(const GemKey& lhs, const GemKey& rhs)
		{
			return lhs.baseId == rhs.baseId && lhs.uniqueId == rhs.uniqueId;
		}
	};

	struct GemKeyHash
	{
		std::size_t operator()(const GemKey& key) const noexcept
		{
			return (static_cast<std::size_t>(key.baseId) << 16) ^ key.uniqueId;
		}
	};

	struct StoredSpellData
	{
		RE::FormID spellId{};
		SpellTier tier{};
		std::int32_t usesRemaining{};
		float lastUsedGameTime{};
		bool isReusableStar{};
		bool isBlackSoulGem{};
	};

	class Serialization
	{
	public:
		static Serialization& GetSingleton();

		void Initialize(const SKSE::SerializationInterface* serialization);
		void Save(SKSE::SerializationInterface* serialization);
		void Load(SKSE::SerializationInterface* serialization);
		void Revert();

		bool HasStoredSpell(const GemKey& key) const;
		const StoredSpellData* GetStoredSpell(const GemKey& key) const;
		void StoreSpell(const GemKey& key, const StoredSpellData& data);
		void RemoveStoredSpell(const GemKey& key);
		bool TryGetStoredSpellByBaseId(RE::FormID baseId, GemKey& key, StoredSpellData& data) const;
		const std::unordered_map<GemKey, StoredSpellData, GemKeyHash>& GetStoredSpells() const;

		std::uint16_t AllocateUniqueId();

	private:
		static void OnSave(SKSE::SerializationInterface* serialization);
		static void OnLoad(SKSE::SerializationInterface* serialization);
		static void OnRevert(SKSE::SerializationInterface* serialization);

		std::unordered_map<GemKey, StoredSpellData, GemKeyHash> storedSpells_;
		std::uint16_t nextUniqueId_{ 1 };
	};
}
