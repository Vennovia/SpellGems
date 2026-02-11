/*=============================================================================================================*/
//																											   //
//                                                  Spell Gems                                                 //
//                                              Spell Gem Manager                                              //
//                                                                                                             //
/*=============================================================================================================*/


#include "SpellGems/SpellGemManager.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include "RE/A/Actor.h"
#include "RE/A/ActorValueOwner.h"
#include "RE/A/ActorValues.h"
#include "RE/B/BSFixedString.h"
#include "RE/E/ExtraUniqueID.h"
#include "RE/E/Effect.h"
#include "RE/E/EffectSetting.h"
#include "RE/C/Calendar.h"
#include "RE/I/InventoryMenu.h"
#include "RE/I/ItemList.h"
#include "RE/I/ItemRemoveReason.h"
#include "RE/M/MagicCaster.h"
#include "RE/M/MagicSystem.h"
#include "RE/M/MemoryManager.h"
#include "RE/M/Misc.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/S/ScriptEventSourceHolder.h"
#include "RE/T/TESObjectMISC.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESDataHandler.h"
#include "RE/U/UI.h"
#include "SKSE/API.h"

#ifdef GetObject
#undef GetObject
#endif

namespace SpellGems
{
	// Returns the singleton spell gem manager instance.
	SpellGemManager& SpellGemManager::GetSingleton()
	{
		static SpellGemManager instance;
		return instance;
	}

	// Registers for container change events to detect stored gem usage.
	void SpellGemManager::RegisterUseEventSink()
	{
		auto* sources = RE::ScriptEventSourceHolder::GetSingleton();
		if (!sources) {
			logger::info("ScriptEventSourceHolder unavailable; cannot register stored gem use handler.");
			return;
		}

		sources->AddEventSink<RE::TESContainerChangedEvent>(&useEventSink_);
		logger::info("Registered stored gem use event handler.");
	}

	// Registers activation hotkeys for stored gem slots.
	void SpellGemManager::RegisterActivationKeys()
	{
		auto* keyHandler = KeyHandler::GetSingleton();
		if (!keyHandler) {
			logger::info("KeyHandler unavailable; activation keys not registered.");
			return;
		}

		for (const auto handle : activationHandles_) {
			keyHandler->Unregister(handle);
		}
		activationHandles_.clear();
		for (const auto handle : activationReleaseHandles_) {
			keyHandler->Unregister(handle);
		}
		activationReleaseHandles_.clear();

		const auto& config = Config::GetSingleton();
		const auto maxStored = config.GetMaxStoredGems();
		for (std::size_t i = 0; i < maxStored; ++i) {
			const auto activationKey = config.GetActivationKey(i);
			if (activationKey == 0) {
				continue;
			}

			auto handle = keyHandler->Register(activationKey, KeyEventType::KEY_DOWN, [i]() {
				SpellGemManager::GetSingleton().ActivateStoredGemSlot(i);
			});
			activationHandles_.push_back(handle);
			auto releaseHandle = keyHandler->Register(activationKey, KeyEventType::KEY_UP, [i]() {
				SpellGemManager::GetSingleton().StopFocusSpellCast(i);
			});
			activationReleaseHandles_.push_back(releaseHandle);
			logger::info("Activation key {} registered: {}", i + 1, activationKey);
		}
	}

	// Activates a stored spell from the specified slot.
	void SpellGemManager::ActivateStoredGemSlot(std::size_t index)
	{
		RefreshStoredGemSlots();
		if (index >= storedGemSlots_.size()) {
			logger::info("No stored spell gem in slot {}.", index + 1);
			return;
		}

		auto& serialization = Serialization::GetSingleton();
		const auto key = storedGemSlots_[index];
		const auto* stored = serialization.GetStoredSpell(key);
		if (!stored) {
			logger::info("Stored spell entry missing for slot {}.", index + 1);
			RefreshStoredGemSlots();
			return;
		}

		auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(stored->spellId);
		if (!spell) {
			logger::info("Stored spell form {:08X} missing for slot {}.", stored->spellId, index + 1);
			return;
		}

		auto* calendar = RE::Calendar::GetSingleton();
		const auto cooldownSeconds = stored->isReusableStar ?
			Config::GetSingleton().GetStarCooldown() :
			Config::GetSingleton().GetTierSettings(stored->tier).cooldown;
		const float timescale = calendar ? calendar->GetTimescale() : 1.0f;
		const float cooldownDays = (cooldownSeconds / (60.0f * 60.0f * 24.0f)) * timescale;
		const float now = calendar ? calendar->GetCurrentGameTime() : 0.0f;
		if (stored->lastUsedGameTime > 0.0f && now - stored->lastUsedGameTime < cooldownDays) {
			const float remainingDays = cooldownDays - (now - stored->lastUsedGameTime);
			const float remainingSeconds = remainingDays * (60.0f * 60.0f * 24.0f) / timescale;
			logger::info("Stored spell gem on cooldown: {:.1f}s remaining.", remainingSeconds);
			LogMessage("Stored spell gem is on cooldown.");
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		const bool isConcentration = spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration;
		std::uint64_t focusId = 0;
		if (isConcentration) {
			activeFocusSlot_ = index;
			focusId = ++focusCastId_;
		}

		const bool isAzurasStar = IsAzurasStar(key.baseId);
		CastStoredSpell(*spell, *player, stored->isBlackSoulGem, stored->isReusableStar, isAzurasStar);
		if (isConcentration) {
			const auto duration = Config::GetSingleton().GetFocusSpellDuration();
			if (duration <= 0.0f) {
				StopFocusSpellCast(index);
			} else {
				std::thread([focusId, duration, index]() {
					std::this_thread::sleep_for(std::chrono::duration<float>(duration));
					if (auto* task = SKSE::GetTaskInterface()) {
						task->AddTask([focusId, index]() {
							auto& manager = SpellGemManager::GetSingleton();
							if (manager.focusCastId_.load() == focusId && manager.activeFocusSlot_ && *manager.activeFocusSlot_ == index) {
								manager.StopFocusSpellCast(index);
							}
						});
					}
				}).detach();
			}
		}
		StoredSpellData updated = *stored;
		updated.lastUsedGameTime = now;
		auto* baseGem = RE::TESForm::LookupByID<RE::TESSoulGem>(key.baseId);
		if (baseGem) {
			ConsumeStoredGemUse(*baseGem, key, updated);
			RefreshStoredGemSlots();
		}
	}

	bool SpellGemManager::ResolveStoredGemSpell(RE::TESForm* form, GemKey& key, StoredSpellData& data, RE::SpellItem*& spell) const
	{
		if (!form || form->GetFormType() != RE::FormType::SoulGem) {
			return false;
		}

		auto& serialization = Serialization::GetSingleton();
		if (!serialization.TryGetStoredSpellByBaseId(form->GetFormID(), key, data)) {
			return false;
		}

		spell = RE::TESForm::LookupByID<RE::SpellItem>(data.spellId);
		return spell != nullptr;
	}

	void SpellGemManager::ConsumeStoredGemUse(RE::TESSoulGem& baseGem, const GemKey& key, const StoredSpellData& data)
	{
		if (data.usesRemaining < 0) {
			Serialization::GetSingleton().StoreSpell(key, data);
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(data.spellId);
		if (!spell) {
			return;
		}

		auto& serialization = Serialization::GetSingleton();
		const auto newUses = data.usesRemaining - 1;
		if (newUses <= 0) {
			serialization.RemoveStoredSpell(key);
			player->RemoveItem(&baseGem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
			GrantFragmentsToPlayer(GetGemTier(baseGem));
			logger::info("Stored spell gem depleted and removed.");
			return;
		}

		StoredSpellData newData = data;
		newData.usesRemaining = newUses;
		serialization.StoreSpell(key, newData);
		logger::info("Stored spell gem uses remaining: {}", newUses);
	}

	void SpellGemManager::RefreshStoredGemSlots()
	{
		const auto& storedSpells = Serialization::GetSingleton().GetStoredSpells();
		storedGemSlots_.clear();
		storedGemSlots_.reserve(storedSpells.size());
		for (const auto& [key, _] : storedSpells) {
			storedGemSlots_.push_back(key);
		}

		std::sort(storedGemSlots_.begin(), storedGemSlots_.end(), [](const GemKey& a, const GemKey& b) {
			if (a.baseId != b.baseId) {
				return a.baseId < b.baseId;
			}
			return a.uniqueId < b.uniqueId;
		});

		const auto maxStored = Config::GetSingleton().GetMaxStoredGems();
		if (storedGemSlots_.size() > maxStored) {
			storedGemSlots_.resize(maxStored);
		}
	}

	// Attempts to store the selected spell into the selected soul gem.
	void SpellGemManager::TryStoreSelectedSpell()
	{
		logger::info("Attempting to store spell in selected soul gem.");
		const auto selected = GetSelectedSoulGem();
		if (!selected.entry) {
			LogMessage("No soul gem selected in inventory.");
			return;
		}

		auto* object = selected.entry->GetObject();
		auto* soulGem = object ? object->As<RE::TESSoulGem>() : nullptr;
		if (!soulGem) {
			LogMessage("Selected item is not a soul gem.");
			return;
		}

		logger::info("Selected soul gem form {:08X}.", soulGem->GetFormID());
		const bool isStarBase = IsReusableStar(soulGem->GetFormID());
		const bool isBlackGemBase = IsBlackSoulGem(*soulGem);
		GemKey existingKey{};
		StoredSpellData existingData{};
		const bool hasExisting = Serialization::GetSingleton().TryGetStoredSpellByBaseId(soulGem->GetFormID(), existingKey, existingData);
		const bool isReusableStar = isStarBase || (hasExisting && existingData.isReusableStar);
		const bool isBlackSoulGem = isBlackGemBase || (hasExisting && existingData.isBlackSoulGem);
		const auto soulLevel = selected.entry ? selected.entry->GetSoulLevel() : RE::SOUL_LEVEL::kNone;
		const bool requireFilled = Config::GetSingleton().RequireFilledSoulGem();
		const bool allowAnyGemTier = requireFilled && Config::GetSingleton().AllowAnyGemTier();
		const bool requireSoul = !isReusableStar && (requireFilled || allowAnyGemTier);
		if (requireSoul && soulLevel == RE::SOUL_LEVEL::kNone) {
			LogMessage("Soul gem must be filled to store a spell.");
			return;
		}

		auto* spell = GetRightHandSpell();
		if (!spell) {
			LogMessage("No right-hand spell equipped.");
			return;
		}

		logger::info("Right-hand spell form {:08X}.", spell->GetFormID());

		SpellTier spellTier{};
		if (!TryGetSpellTier(*spell, spellTier)) {
			LogMessage("Unable to determine spell tier for the selected spell.");
			return;
		}
		if (requireSoul && soulLevel != RE::SOUL_LEVEL::kNone) {
			const auto requiredSoul = [&]() {
				switch (spellTier) {
				case SpellTier::Novice:
					return RE::SOUL_LEVEL::kPetty;
				case SpellTier::Apprentice:
					return RE::SOUL_LEVEL::kLesser;
				case SpellTier::Adept:
					return RE::SOUL_LEVEL::kCommon;
				case SpellTier::Expert:
					return RE::SOUL_LEVEL::kGreater;
				case SpellTier::Master:
					return RE::SOUL_LEVEL::kGrand;
				default:
					return RE::SOUL_LEVEL::kPetty;
				}
			}();
			if (static_cast<std::uint8_t>(soulLevel) < static_cast<std::uint8_t>(requiredSoul)) {
				const auto requiredName = [&]() {
					switch (requiredSoul) {
					case RE::SOUL_LEVEL::kPetty:
						return "Petty";
					case RE::SOUL_LEVEL::kLesser:
						return "Lesser";
					case RE::SOUL_LEVEL::kCommon:
						return "Common";
					case RE::SOUL_LEVEL::kGreater:
						return "Greater";
					case RE::SOUL_LEVEL::kGrand:
						return "Grand";
					default:
						return "Petty";
					}
				}();
				LogMessage(std::string("Soul gem must contain at least a ") + requiredName + " soul.");
				return;
			}
		}
		const auto gemTier = GetGemTier(*soulGem);
		logger::info("Spell tier {} vs gem tier {}.", static_cast<int>(spellTier), static_cast<int>(gemTier));
		if (!isReusableStar && !allowAnyGemTier && gemTier != spellTier) {
			LogMessage("Soul gem tier must match the spell tier.");
			return;
		}

		auto& serialization = Serialization::GetSingleton();
		const auto& config = Config::GetSingleton();
		const auto& tierSettings = config.GetTierSettings(spellTier);
		StoredSpellData data{};
		data.spellId = spell->GetFormID();
		data.tier = spellTier;
		data.usesRemaining = isReusableStar ? -1 : (config.IsFiniteUse() ? tierSettings.uses : -1);
		data.lastUsedGameTime = 0.0f;
		data.isReusableStar = isReusableStar;
		data.isBlackSoulGem = isBlackSoulGem;

		RE::TESSoulGem* storedGemForm = nullptr;
		if (isReusableStar && hasExisting && existingData.isReusableStar) {
			storedGemForm = soulGem;
		} else {
			storedGemForm = GetOrCreateStoredGemForm(*soulGem, *spell, spellTier, data.usesRemaining);
			if (!storedGemForm) {
				LogMessage("Failed to create stored spell gem form.");
				return;
			}
		}

		auto* newExtraList = selected.extraList;
		GemKey key{};
		if (isReusableStar && hasExisting && existingData.isReusableStar) {
			key = existingKey;
		} else {
			const auto uniqueId = newExtraList ?
				GetOrCreateUniqueId(*storedGemForm, *newExtraList) :
				serialization.AllocateUniqueId();
			key = { storedGemForm->GetFormID(), uniqueId };
			if (serialization.HasStoredSpell(key)) {
				LogMessage("Soul gem already contains a spell.");
				return;
			}
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			LogMessage("Player reference unavailable.");
			return;
		}

		logger::info("Removing selected soul gem from inventory.");
		if (!(isReusableStar && hasExisting && existingData.isReusableStar)) {
			player->RemoveItem(soulGem, 1, RE::ITEM_REMOVE_REASON::kRemove, selected.extraList, nullptr);
			logger::info("Adding stored spell gem to inventory.");
			player->AddObjectToContainer(storedGemForm, newExtraList, 1, player);
			logger::info("Inventory swap complete.");
		}

		serialization.StoreSpell(key, data);
		logger::info("Stored spell gem form {:08X} added to player.", storedGemForm->GetFormID());
		RefreshStoredGemSlots();

		LogMessage("Stored spell in soul gem.");
	}

	SpellGemManager::SelectedGem SpellGemManager::GetSelectedSoulGem() const
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui || !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
			return { nullptr, nullptr };
		}

		auto menu = ui->GetMenu<RE::InventoryMenu>();
		if (!menu) {
			return { nullptr, nullptr };
		}

		auto& data = menu->GetRuntimeData();
		if (!data.itemList) {
			return { nullptr, nullptr };
		}

		auto* selectedItem = data.itemList->GetSelectedItem();
		if (!selectedItem || !selectedItem->data.objDesc) {
			return { nullptr, nullptr };
		}

		auto* entry = selectedItem->data.objDesc;
		RE::ExtraDataList* extraList = nullptr;
		if (entry->extraLists) {
			auto iter = entry->extraLists->begin();
			if (iter != entry->extraLists->end()) {
				extraList = *iter;
			}
		}

		return { entry, extraList };
	}

	RE::SpellItem* SpellGemManager::GetRightHandSpell() const
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return nullptr;
		}

		auto* equipped = player->GetEquippedObject(false);
		return equipped ? equipped->As<RE::SpellItem>() : nullptr;
	}

	SpellTier SpellGemManager::GetSpellTier(const RE::SpellItem& spell) const
	{
		const auto* effect = spell.GetCostliestEffectItem();
		const auto* baseEffect = effect ? effect->baseEffect : nullptr;
		const auto minSkill = baseEffect ? baseEffect->GetMinimumSkillLevel() : 0;

		if (minSkill >= 100) {
			return SpellTier::Master;
		}
		if (minSkill >= 75) {
			return SpellTier::Expert;
		}
		if (minSkill >= 50) {
			return SpellTier::Adept;
		}
		if (minSkill >= 25) {
			return SpellTier::Apprentice;
		}
		return SpellTier::Novice;
	}

	bool SpellGemManager::TryGetSpellTier(const RE::SpellItem& spell, SpellTier& tier) const
	{
		const auto* effect = spell.GetCostliestEffectItem();
		const auto* baseEffect = effect ? effect->baseEffect : nullptr;
		if (!baseEffect) {
			logger::info("Spell {:08X} has no base effect; cannot determine tier.", spell.GetFormID());
			return false;
		}

		tier = GetSpellTier(spell);
		return true;
	}

	SpellTier SpellGemManager::GetGemTier(const RE::TESSoulGem& gem) const
	{
		if (gem.CanHoldNPCSoul()) {
			return SpellTier::Master;
		}

		switch (gem.GetMaximumCapacity()) {
		case RE::SOUL_LEVEL::kGrand:
			return SpellTier::Master;
		case RE::SOUL_LEVEL::kGreater:
			return SpellTier::Expert;
		case RE::SOUL_LEVEL::kCommon:
			return SpellTier::Adept;
		case RE::SOUL_LEVEL::kLesser:
			return SpellTier::Apprentice;
		case RE::SOUL_LEVEL::kPetty:
			return SpellTier::Novice;
		default:
			return SpellTier::Novice;
		}
	}

	std::uint16_t SpellGemManager::GetOrCreateUniqueId(const RE::TESSoulGem& gem, RE::ExtraDataList& extraList) const
	{
		auto* uniqueData = extraList.GetByType<RE::ExtraUniqueID>();
		if (uniqueData) {
			uniqueData->baseID = gem.GetFormID();
			return uniqueData->uniqueID;
		}

		auto uniqueId = Serialization::GetSingleton().AllocateUniqueId();
		auto* newUnique = new RE::ExtraUniqueID(gem.GetFormID(), uniqueId);
		extraList.Add(newUnique);
		return uniqueId;
	}

	void SpellGemManager::LogMessage(const std::string& message) const
	{
		logger::info("{}", message);
		RE::DebugNotification(message.c_str());
	}

	RE::ExtraDataList* SpellGemManager::CreateExtraDataList() const
	{
		auto* list = RE::malloc<RE::ExtraDataList>();
		if (!list) {
			return nullptr;
		}

		std::memset(list, 0, sizeof(RE::ExtraDataList));

		auto* base = reinterpret_cast<RE::BaseExtraList*>(list);
		base->GetPresence() = new RE::BaseExtraList::PresenceBitfield{};
		std::memset(base->GetPresence(), 0, sizeof(RE::BaseExtraList::PresenceBitfield));
		return list;
	}

	RE::TESSoulGem* SpellGemManager::GetOrCreateStoredGemForm(RE::TESSoulGem& baseGem, const RE::SpellItem& spell, SpellTier tier, std::int32_t usesRemaining)
	{
		StoredGemFormKey key{ baseGem.GetFormID(), spell.GetFormID(), tier, usesRemaining };
		if (auto it = storedGemForms_.find(key); it != storedGemForms_.end()) {
			return it->second;
		}

		auto* duplicated = baseGem.CreateDuplicateForm(false, nullptr);
		auto* storedGem = duplicated ? duplicated->As<RE::TESSoulGem>() : nullptr;
		if (!storedGem) {
			logger::info("Failed to duplicate soul gem form {:08X}.", baseGem.GetFormID());
			return nullptr;
		}

		const auto displayName = BuildDisplayName(spell, tier);
		storedGem->SetFullName(displayName.c_str());

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (dataHandler && !dataHandler->AddFormToDataHandler(storedGem)) {
			logger::info("Failed to register stored gem form with data handler.");
		}

		storedGemForms_.emplace(key, storedGem);
		logger::info("Created stored spell gem form {:08X} for spell {:08X}.", storedGem->GetFormID(), spell.GetFormID());
		return storedGem;
	}

	std::string SpellGemManager::BuildDisplayName(const RE::SpellItem& spell, SpellTier tier) const
	{
		const auto* spellName = spell.GetName();
		const std::string name = (spellName && spellName[0] != '\0') ? spellName : "Unknown Spell";
		const auto tierName = Config::GetTierName(tier);
		return name + " (" + std::string(tierName) + ")";
	}

	RE::BSEventNotifyControl SpellGemManager::StoredGemUseEventSink::ProcessEvent(
		const RE::TESContainerChangedEvent* event,
		RE::BSTEventSource<RE::TESContainerChangedEvent>*)
	{
		if (!event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		return manager_.HandleContainerChanged(*event);
	}

	// Handles stored gem consumption events from the player's inventory.
	RE::BSEventNotifyControl SpellGemManager::HandleContainerChanged(const RE::TESContainerChangedEvent& event)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (event.oldContainer != player->GetFormID() || event.newContainer != 0 || event.itemCount >= 0) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (event.reference) {
			return RE::BSEventNotifyControl::kContinue;
		}

		const GemKey key{ event.baseObj, event.uniqueID };
		auto& serialization = Serialization::GetSingleton();
		const auto* stored = serialization.GetStoredSpell(key);
		if (!stored) {
			return RE::BSEventNotifyControl::kContinue;
		}

		auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(stored->spellId);
		if (!spell) {
			logger::info("Stored spell form {:08X} not found for used gem.", stored->spellId);
			serialization.RemoveStoredSpell(key);
			return RE::BSEventNotifyControl::kContinue;
		}

	SpellTier effectiveTier = stored->tier;
	if (TryGetSpellTier(*spell, effectiveTier) && effectiveTier != stored->tier) {
		StoredSpellData updated = *stored;
		updated.tier = effectiveTier;
		serialization.StoreSpell(key, updated);
	}

		logger::info("Stored spell gem used: {:08X} (unique {}).", key.baseId, key.uniqueId);
		const bool isAzurasStar = IsAzurasStar(event.baseObj);
		CastStoredSpell(*spell, *player, stored->isBlackSoulGem, stored->isReusableStar, isAzurasStar);

		std::int32_t newUses = stored->usesRemaining;
		if (newUses > 0) {
			newUses -= 1;
		}

		if (newUses == 0) {
			serialization.RemoveStoredSpell(key);
			auto* baseGem = RE::TESForm::LookupByID<RE::TESSoulGem>(event.baseObj);
			player->RemoveItem(baseGem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
			if (baseGem) {
				GrantFragmentsToPlayer(GetGemTier(*baseGem));
			}
			logger::info("Stored spell gem depleted and consumed.");
			return RE::BSEventNotifyControl::kContinue;
		}

		StoredSpellData newData = *stored;
		newData.usesRemaining = newUses;
		serialization.StoreSpell(key, newData);
		logger::info("Stored spell gem uses remaining: {}", newUses);

		return RE::BSEventNotifyControl::kContinue;
	}

	// Casts the stored spell with any gem-specific modifiers.
	void SpellGemManager::CastStoredSpell(RE::SpellItem& spell, RE::PlayerCharacter& player, bool isBlackSoulGem, bool isReusableStar, bool isAzurasStar)
	{
		auto* caster = player.GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
		if (!caster) {
			caster = player.GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand);
		}
		if (!caster) {
			caster = player.GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		}
		if (!caster) {
			logger::info("Magic caster unavailable for stored spell cast.");
			return;
		}

		RE::TESObjectREFR* target = nullptr;
		if (spell.GetDelivery() == RE::MagicSystem::Delivery::kSelf) {
			target = &player;
		}

		float effectiveness = 1.0f;
		float magnitudeOverride = 0.0f;
		if (isAzurasStar && Config::GetSingleton().AzurasStarBoost()) {
			effectiveness = 1.05f;
			magnitudeOverride = 1.05f;
		} else if (!isBlackSoulGem && !isReusableStar && Config::GetSingleton().NormalGemPenalty()) {
			effectiveness = 0.9f;
			magnitudeOverride = 0.9f;
		} else if (isBlackSoulGem && Config::GetSingleton().BlackSoulGemBoosts()) {
			bool hasDestruction = false;
			bool hasDurationBoost = false;
			for (const auto* effect : spell.effects) {
				if (!effect || !effect->baseEffect) {
					continue;
				}
				switch (effect->baseEffect->GetMagickSkill()) {
				case RE::ActorValue::kDestruction:
					hasDestruction = true;
					break;
				case RE::ActorValue::kConjuration:
				case RE::ActorValue::kAlteration:
					hasDurationBoost = true;
					break;
				default:
					break;
				}
			}
			if (hasDestruction) {
				magnitudeOverride = 1.1f;
			}
			if (hasDurationBoost) {
				effectiveness = 1.1f;
			}
			if (auto* avOwner = player.AsActorValueOwner()) {
				const auto maxHealth = avOwner->GetPermanentActorValue(RE::ActorValue::kHealth) +
					player.GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth) +
					player.GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
				avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -(maxHealth * 0.05f));
			}
		}

		if (spell.GetCastingType() == RE::MagicSystem::CastingType::kConcentration) {
			player.NotifyAnimationGraph(RE::BSFixedString("MT_BreathExhaleShort"));
		} else {
			player.NotifyAnimationGraph(RE::BSFixedString("ShoutStart"));
		}
		const auto previousCost = caster->currentSpellCost;
		caster->currentSpellCost = 0.0f;
		caster->PrepareSound(RE::MagicSystem::SoundID::kRelease, &spell);
		caster->CastSpellImmediate(&spell, false, target, effectiveness, false, magnitudeOverride, &player);
		caster->PlayReleaseSound(&spell);
		if (spell.GetCastingType() == RE::MagicSystem::CastingType::kConcentration) {
			focusCasterSource_ = caster->GetCastingSource();
			focusPreviousCost_ = previousCost;
			focusCostActive_ = true;
			if (auto* avOwner = player.AsActorValueOwner()) {
				focusStartMagicka_ = avOwner->GetActorValue(RE::ActorValue::kMagicka);
				const auto focusId = focusCastId_.load();
				std::thread([focusId]() {
					while (true) {
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
						if (auto* task = SKSE::GetTaskInterface()) {
							task->AddTask([focusId]() {
								auto& manager = SpellGemManager::GetSingleton();
								if (!manager.focusCostActive_ || manager.focusCastId_.load() != focusId) {
									return;
								}
								if (auto* pc = RE::PlayerCharacter::GetSingleton()) {
										if (auto* avOwner = pc->AsActorValueOwner()) {
											const auto current = avOwner->GetActorValue(RE::ActorValue::kMagicka);
											if (current < manager.focusStartMagicka_) {
												avOwner->ModActorValue(RE::ActorValue::kMagicka, manager.focusStartMagicka_ - current);
											}
										}
									}
								});
						}
						if (!SpellGemManager::GetSingleton().focusCostActive_ || SpellGemManager::GetSingleton().focusCastId_.load() != focusId) {
							break;
						}
					}
				}).detach();
			}
		} else {
			caster->currentSpellCost = previousCost;
		}
		logger::info("Cast stored spell {:08X} via gem activation.", spell.GetFormID());
	}

	// Stops a concentration spell cast started from a stored gem.
	void SpellGemManager::StopFocusSpellCast(std::size_t index)
	{
		if (!activeFocusSlot_ || *activeFocusSlot_ != index) {
			return;
		}

		activeFocusSlot_.reset();
		++focusCastId_;
		if (auto* pc = RE::PlayerCharacter::GetSingleton()) {
			if (auto* caster = pc->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)) {
				caster->InterruptCast(true);
			}
			if (auto* caster = pc->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)) {
				caster->InterruptCast(true);
			}
		}
		if (focusCostActive_ && focusCasterSource_) {
			if (auto* pc = RE::PlayerCharacter::GetSingleton()) {
				if (auto* caster = pc->GetMagicCaster(*focusCasterSource_)) {
					caster->currentSpellCost = focusPreviousCost_;
				}
			}
		}
		focusCostActive_ = false;
		focusCasterSource_.reset();
		focusStartMagicka_ = 0.0f;
	}

	bool SpellGemManager::IsReusableStar(RE::FormID formId) const
	{
		return formId == 0x00063B27 || formId == 0x00063B29;
	}

	bool SpellGemManager::IsAzurasStar(RE::FormID formId) const
	{
		return formId == 0x00063B27;
	}

	bool SpellGemManager::IsBlackSoulGem(const RE::TESSoulGem& gem) const
	{
		const auto formId = gem.GetFormID();
		return formId == 0x00063B29 || gem.CanHoldNPCSoul();
	}
	void SpellGemManager::GrantFragmentsToPlayer(SpellTier tier) const
	{
		const auto count = Config::GetSingleton().GetFragmentCount(tier);
		if (count == 0) {
			return;
		}

		auto* fragmentForm = RE::TESForm::LookupByID<RE::TESObjectMISC>(Config::GetSingleton().GetFragmentFormId());
		if (!fragmentForm) {
			logger::info("Fragment form {:08X} not found; skipping fragments.", Config::GetSingleton().GetFragmentFormId());
			return;
		}

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			player->AddObjectToContainer(fragmentForm, nullptr, static_cast<std::int32_t>(count), player);
			logger::info("Granted {} soul gem fragments.", count);
		}
	}
}
