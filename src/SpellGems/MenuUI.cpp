/*=============================================================================================================*/
//																											   //
//                                                  Spell Gems                                                 //
//                                             Config UI Rendering                                             //
//                                                                                                             //
/*=============================================================================================================*/


// Settings UI rendering using SKSEMenuFramework.
#include "SpellGems/MenuUI.h"

#include "SpellGems/Config.h"
#include "SpellGems/Serialization.h"
#include "SpellGems/SpellGemManager.h"
#include "include/SKSEMenuFramework.h"

#include <string>
#include <vector>

#include "RE/T/TESForm.h"

namespace SpellGems
{
	// Registers the settings UI section when the framework is available.
	void MenuUI::Initialize()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::info("SKSEMenuFramework not installed; settings UI disabled.");
			return;
		}

		SKSEMenuFramework::SetSection("Spell Gems");
		SKSEMenuFramework::AddSectionItem("Settings", &MenuUI::Render);
		logger::info("Spell Gems settings UI registered.");
	}

	// Renders the SpellGems settings panel.
	void MenuUI::Render()
	{
		auto& config = Config::GetSingleton();

		ImGuiMCP::Text("Spell Gems Configuration");
		ImGuiMCP::Separator();

		bool finiteUse = config.IsFiniteUse();
		if (ImGuiMCP::Checkbox("Finite Uses", &finiteUse)) {
			config.SetFiniteUse(finiteUse);
			logger::info("Finite Uses toggled: {}", finiteUse);
			auto& serialization = Serialization::GetSingleton();
			const auto& storedSpells = serialization.GetStoredSpells();
			std::vector<std::pair<GemKey, StoredSpellData>> entries{ storedSpells.begin(), storedSpells.end() };
			for (auto& [key, data] : entries) {
				if (data.isReusableStar) {
					data.usesRemaining = -1;
				} else if (!finiteUse) {
					data.usesRemaining = -1;
				} else if (data.usesRemaining < 0) {
					data.usesRemaining = config.GetTierSettings(data.tier).uses;
				}
				serialization.StoreSpell(key, data);
			}
		}

		bool requireFilled = config.RequireFilledSoulGem();
		if (ImGuiMCP::Checkbox("Require Filled Soul Gem", &requireFilled)) {
			config.SetRequireFilledSoulGem(requireFilled);
			logger::info("Require Filled Soul Gem toggled: {}", requireFilled);
		}

		bool allowAnyGemTier = config.AllowAnyGemTier();
		if (ImGuiMCP::Checkbox("Allow Any Gem Tier (use soul level)", &allowAnyGemTier)) {
			config.SetAllowAnyGemTier(allowAnyGemTier);
			logger::info("Allow Any Gem Tier toggled: {}", allowAnyGemTier);
		}

		bool blackGemBoosts = config.BlackSoulGemBoosts();
		if (ImGuiMCP::Checkbox("Black Soul Gem Boosts", &blackGemBoosts)) {
			config.SetBlackSoulGemBoosts(blackGemBoosts);
			logger::info("Black soul gem boosts toggled: {}", blackGemBoosts);
		}

		bool normalGemPenalty = config.NormalGemPenalty();
		if (ImGuiMCP::Checkbox("Normal Gem Penalty", &normalGemPenalty)) {
			config.SetNormalGemPenalty(normalGemPenalty);
			logger::info("Normal gem penalty toggled: {}", normalGemPenalty);
		}

		bool azurasStarBoost = config.AzurasStarBoost();
		if (ImGuiMCP::Checkbox("Azura's Star Boost", &azurasStarBoost)) {
			config.SetAzurasStarBoost(azurasStarBoost);
			logger::info("Azura's Star boost toggled: {}", azurasStarBoost);
		}

		float focusDuration = config.GetFocusSpellDuration();
		if (ImGuiMCP::SliderFloat("Focus Spell Duration (s)", &focusDuration, 0.0f, 3.0f, "%.1f")) {
			config.SetFocusSpellDuration(focusDuration);
			logger::info("Focus spell duration updated: {}", focusDuration);
		}

		float starCooldown = config.GetStarCooldown();
		if (ImGuiMCP::SliderFloat("Star Cooldown (s)", &starCooldown, 1.0f, 30.0f, "%.1f s")) {
			config.SetStarCooldown(starCooldown);
			logger::info("Star cooldown updated: {}", starCooldown);
		}

		bool showUses = config.ShowUsesRemaining();
		if (ImGuiMCP::Checkbox("Show Uses Remaining", &showUses)) {
			config.SetShowUsesRemaining(showUses);
			logger::info("Show Uses Remaining toggled: {}", showUses);
		}

		int storeKey = static_cast<int>(config.GetStoreKey());
		if (ImGuiMCP::InputInt("Store Spell Key (DIK)", &storeKey, 1, 10)) {
			if (storeKey > 0) {
				config.SetStoreKey(static_cast<std::uint32_t>(storeKey));
				logger::info("Store key updated: {}", storeKey);
			}
		}

		int maxStored = static_cast<int>(config.GetMaxStoredGems());
		if (ImGuiMCP::SliderInt("Max Stored Gems", &maxStored, 1, 5)) {
			config.SetMaxStoredGems(static_cast<std::uint8_t>(maxStored));
			logger::info("Max stored gems updated: {}", maxStored);
			SpellGemManager::GetSingleton().RegisterActivationKeys();
		}

		for (std::size_t i = 0; i < 5; ++i) {
			int activationKey = static_cast<int>(config.GetActivationKey(i));
			std::string label = "Activate Gem " + std::to_string(i + 1) + " Key (DIK)";
			if (ImGuiMCP::InputInt(label.c_str(), &activationKey, 1, 10)) {
				if (activationKey > 0) {
					config.SetActivationKey(i, static_cast<std::uint32_t>(activationKey));
					logger::info("Activation key {} updated: {}", i + 1, activationKey);
				}
			}
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::Separator();
		ImGuiMCP::Text("Tier Settings");

		for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(SpellTier::Total); ++i) {
			auto tier = static_cast<SpellTier>(i);
			auto& tierSettings = config.GetTierSettings(tier);
			const auto label = Config::GetTierName(tier);

			ImGuiMCP::SeparatorText(label.data());

			float cooldown = tierSettings.cooldown;
			if (ImGuiMCP::SliderFloat(std::string(label).append(" Cooldown").c_str(), &cooldown, 1.0f, 30.0f, "%.1f s")) {
				tierSettings.cooldown = cooldown;
				logger::info("{} cooldown updated: {}", label, cooldown);
			}

			int uses = tierSettings.uses;
			if (ImGuiMCP::SliderInt(std::string(label).append(" Uses").c_str(), &uses, 1, 20, "%d")) {
				tierSettings.uses = uses;
				logger::info("{} uses updated: {}", label, uses);
			}

			int fragmentCount = static_cast<int>(config.GetFragmentCount(tier));
			if (ImGuiMCP::SliderInt(std::string(label).append(" Fragment Count").c_str(), &fragmentCount, 0, 10, "%d")) {
				fragmentCount = fragmentCount < 0 ? 0 : fragmentCount;
				config.SetFragmentCount(tier, static_cast<std::uint32_t>(fragmentCount));
				logger::info("{} fragment count updated: {}", label, fragmentCount);
			}
		}

		ImGuiMCP::Spacing();
		if (ImGuiMCP::Button("Save Settings")) {
			config.Save();
			logger::info("Settings saved from UI.");
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::Separator();
		auto& serialization = Serialization::GetSingleton();
		const auto& storedSpells = serialization.GetStoredSpells();
		static std::vector<std::pair<GemKey, StoredSpellData>> cachedSpells;
		static bool needsRefresh = true;
		if (needsRefresh || cachedSpells.size() != storedSpells.size()) {
			cachedSpells.assign(storedSpells.begin(), storedSpells.end());
			needsRefresh = false;
		}

		ImGuiMCP::Text("Stored Spell Gems (%zu)", cachedSpells.size());
		if (ImGuiMCP::Button("Refresh List")) {
			needsRefresh = true;
		}

		if (ImGuiMCP::BeginTable("StoredSpellGems", 7)) {
			ImGuiMCP::TableSetupColumn("Slot");
			ImGuiMCP::TableSetupColumn("Gem");
			ImGuiMCP::TableSetupColumn("Spell");
			ImGuiMCP::TableSetupColumn("Uses");
			ImGuiMCP::TableSetupColumn("Cooldown");
			ImGuiMCP::TableSetupColumn("Key (DIK)");
			ImGuiMCP::TableSetupColumn("Actions");
			ImGuiMCP::TableHeadersRow();

			std::size_t slotIndex = 0;
			for (const auto& [key, data] : cachedSpells) {
				const auto* gemForm = RE::TESForm::LookupByID(key.baseId);
				const auto* spellForm = RE::TESForm::LookupByID(data.spellId);
				const char* gemName = gemForm ? gemForm->GetName() : "Unknown Gem";
				const char* spellName = spellForm ? spellForm->GetName() : "Unknown Spell";
				const auto tierName = Config::GetTierName(data.tier);
				bool removed = false;

				ImGuiMCP::TableNextRow();
				ImGuiMCP::TableNextColumn();
				ImGuiMCP::Text("%zu", slotIndex + 1);
				ImGuiMCP::TableNextColumn();
				ImGuiMCP::Text("%s", gemName);
				ImGuiMCP::TableNextColumn();
				ImGuiMCP::Text("%s (%s)", spellName, tierName.data());
				ImGuiMCP::TableNextColumn();
				if (data.usesRemaining < 0) {
					ImGuiMCP::Text("Infinite");
				} else {
					ImGuiMCP::Text("%d", data.usesRemaining);
				}
				ImGuiMCP::TableNextColumn();
				const auto& tierSettings = config.GetTierSettings(data.tier);
				ImGuiMCP::Text("%.1f s", tierSettings.cooldown);
				ImGuiMCP::TableNextColumn();
				int activationKey = static_cast<int>(config.GetActivationKey(slotIndex));
				ImGuiMCP::PushID(static_cast<int>(key.baseId ^ (key.uniqueId << 1)));
				if (ImGuiMCP::InputInt("##key", &activationKey, 1, 10)) {
					if (activationKey > 0) {
						config.SetActivationKey(slotIndex, static_cast<std::uint32_t>(activationKey));
						logger::info("Activation key {} updated: {}", slotIndex + 1, activationKey);
					}
				}
				ImGuiMCP::TableNextColumn();
				if (ImGuiMCP::Button("Remove")) {
					serialization.RemoveStoredSpell(key);
					removed = true;
				}
				ImGuiMCP::PopID();
				if (removed) {
					needsRefresh = true;
					break;
				}

				++slotIndex;
				if (slotIndex >= config.GetMaxStoredGems()) {
					break;
				}
			}

			ImGuiMCP::EndTable();
		}
	}
}
