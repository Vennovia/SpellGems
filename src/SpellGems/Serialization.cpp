/*=============================================================================================================*/
//																											   //
//                                                  Spell Gems                                                 //
//                                              SKSE Serialization                                             //
//                                                                                                             //
/*=============================================================================================================*/


#include "SpellGems/Serialization.h"

#include <algorithm>
#include <vector>

#include "SKSE/API.h"

namespace SpellGems
{
	namespace
	{
		constexpr std::uint32_t kSerializationVersion = 3;
		constexpr std::uint32_t kPluginId = 'SGEM';
		constexpr std::uint32_t kRecordSpells = 'SPEL';
		constexpr std::uint32_t kRecordState = 'STAT';
	}

	// Returns the singleton serialization manager.
	Serialization& Serialization::GetSingleton()
	{
		static Serialization instance;
		return instance;
	}

	// Registers serialization callbacks with SKSE.
	void Serialization::Initialize(const SKSE::SerializationInterface* serialization)
	{
		if (!serialization) {
			logger::info("Serialization interface unavailable.");
			return;
		}

		serialization->SetUniqueID(kPluginId);
		serialization->SetSaveCallback(&Serialization::OnSave);
		serialization->SetLoadCallback(&Serialization::OnLoad);
		serialization->SetRevertCallback(&Serialization::OnRevert);
		logger::info("Serialization callbacks set.");
	}

	// Writes stored spell data to the save file.
	void Serialization::Save(SKSE::SerializationInterface* serialization)
	{
		if (!serialization) {
			logger::info("Serialization save skipped (null interface).");
			return;
		}

		logger::info("Saving {} stored spell entries.", storedSpells_.size());

		if (serialization->OpenRecord(kRecordState, kSerializationVersion)) {
			serialization->WriteRecordData(nextUniqueId_);
		}

		if (serialization->OpenRecord(kRecordSpells, kSerializationVersion)) {
			const std::uint32_t count = static_cast<std::uint32_t>(storedSpells_.size());
			serialization->WriteRecordData(count);

			for (const auto& [key, data] : storedSpells_) {
				serialization->WriteRecordData(key.baseId);
				serialization->WriteRecordData(key.uniqueId);
				serialization->WriteRecordData(data.spellId);
				serialization->WriteRecordData(data.tier);
				serialization->WriteRecordData(data.usesRemaining);
				serialization->WriteRecordData(data.lastUsedGameTime);
				serialization->WriteRecordData(data.isReusableStar);
				serialization->WriteRecordData(data.isBlackSoulGem);
			}
		}
	}

	// Restores stored spell data from the save file.
	void Serialization::Load(SKSE::SerializationInterface* serialization)
	{
		if (!serialization) {
			logger::info("Serialization load skipped (null interface).");
			return;
		}

		storedSpells_.clear();
		logger::info("Loading stored spell data.");

		std::uint32_t type = 0;
		std::uint32_t version = 0;
		std::uint32_t length = 0;
		while (serialization->GetNextRecordInfo(type, version, length)) {
			switch (type) {
			case kRecordState:
				serialization->ReadRecordData(nextUniqueId_);
				break;
			case kRecordSpells: {
				std::uint32_t count = 0;
				serialization->ReadRecordData(count);
				for (std::uint32_t i = 0; i < count; ++i) {
					GemKey key{};
					StoredSpellData data{};
					serialization->ReadRecordData(key.baseId);
					serialization->ReadRecordData(key.uniqueId);
					serialization->ReadRecordData(data.spellId);
					serialization->ReadRecordData(data.tier);
					serialization->ReadRecordData(data.usesRemaining);
					serialization->ReadRecordData(data.lastUsedGameTime);
					if (version >= 2) {
						serialization->ReadRecordData(data.isReusableStar);
					} else {
						data.isReusableStar = false;
					}
					if (version >= 3) {
						serialization->ReadRecordData(data.isBlackSoulGem);
					} else {
						data.isBlackSoulGem = false;
					}

					RE::FormID resolvedSpell = 0;
					if (!serialization->ResolveFormID(data.spellId, resolvedSpell)) {
						continue;
					}
					data.spellId = resolvedSpell;

					RE::FormID resolvedGem = 0;
					if (!serialization->ResolveFormID(key.baseId, resolvedGem)) {
						continue;
					}
					key.baseId = resolvedGem;
					storedSpells_.emplace(key, data);
				}
				break;
			}
			default: {
				std::vector<std::uint8_t> buffer(length);
				serialization->ReadRecordData(buffer.data(), length);
				break;
			}
			}
		}
	}

	// Clears runtime spell data when a save is reverted.
	void Serialization::Revert()
	{
		storedSpells_.clear();
		nextUniqueId_ = 1;
		logger::info("Serialization revert complete.");
	}

	bool Serialization::HasStoredSpell(const GemKey& key) const
	{
		return storedSpells_.contains(key);
	}

	const StoredSpellData* Serialization::GetStoredSpell(const GemKey& key) const
	{
		auto it = storedSpells_.find(key);
		if (it == storedSpells_.end()) {
			return nullptr;
		}

		return std::addressof(it->second);
	}

	void Serialization::StoreSpell(const GemKey& key, const StoredSpellData& data)
	{
		storedSpells_[key] = data;
		logger::info("Stored spell {} in gem {:08X} (unique {}).", data.spellId, key.baseId, key.uniqueId);
	}

	void Serialization::RemoveStoredSpell(const GemKey& key)
	{
		if (storedSpells_.erase(key) > 0) {
			logger::info("Removed stored spell from gem {:08X} (unique {}).", key.baseId, key.uniqueId);
		}
	}

	bool Serialization::TryGetStoredSpellByBaseId(RE::FormID baseId, GemKey& key, StoredSpellData& data) const
	{
		for (const auto& [storedKey, storedData] : storedSpells_) {
			if (storedKey.baseId == baseId) {
				key = storedKey;
				data = storedData;
				return true;
			}
		}
		return false;
	}

	const std::unordered_map<GemKey, StoredSpellData, GemKeyHash>& Serialization::GetStoredSpells() const
	{
		return storedSpells_;
	}

	std::uint16_t Serialization::AllocateUniqueId()
	{
		return nextUniqueId_++;
	}

	// SKSE save callback entry point.
	void Serialization::OnSave(SKSE::SerializationInterface* serialization)
	{
		GetSingleton().Save(serialization);
	}

	// SKSE load callback entry point.
	void Serialization::OnLoad(SKSE::SerializationInterface* serialization)
	{
		GetSingleton().Load(serialization);
	}

	// SKSE revert callback entry point.
	void Serialization::OnRevert(SKSE::SerializationInterface*)
	{
		GetSingleton().Revert();
	}
}
