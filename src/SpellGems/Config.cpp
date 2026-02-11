/*=============================================================================================================*/
//																											   //
//                                                  Spell Gems                                                 //
//                                                Config Settings                                              //
//                                                                                                             //
/*=============================================================================================================*/


#include "SpellGems/Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace SpellGems
{
	namespace
	{
		constexpr std::array<const char*, static_cast<std::size_t>(SpellTier::Total)> kTierNames{
			"Novice",
			"Apprentice",
			"Adept",
			"Expert",
			"Master"
		};

		// Returns the path to the SpellGems INI file.
		std::filesystem::path GetConfigPath()
		{
			return std::filesystem::path("Data/SKSE/Plugins") / "SpellGems.ini";
		}

		// Trims leading and trailing whitespace from a string.
		std::string Trim(const std::string& value)
		{
			auto begin = value.find_first_not_of(" \t\r\n");
			if (begin == std::string::npos) {
				return {};
			}
			auto end = value.find_last_not_of(" \t\r\n");
			return value.substr(begin, end - begin + 1);
		}

		// Parses a boolean value from common string representations.
		bool TryParseBool(const std::string& value, bool& out)
		{
			const auto lowered = [&]() {
				std::string result = value;
				std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
				return result;
			}();

			if (lowered == "true" || lowered == "1" || lowered == "yes") {
				out = true;
				return true;
			}
			if (lowered == "false" || lowered == "0" || lowered == "no") {
				out = false;
				return true;
			}

			return false;
		}
	}

	// Initializes configuration defaults.
	Config::Config()
	{
		storeKey_ = 0x4C;
		activationKeys_ = { 2, 3, 4, 5, 6 };
		maxStoredGems_ = 5;
		requireFilledSoulGem_ = true;
		allowAnyGemTier_ = false;
		blackSoulGemBoosts_ = true;
		normalGemPenalty_ = true;
		azurasStarBoost_ = true;
		focusSpellDuration_ = 2.0f;
		starCooldown_ = 3.0f;
		fragmentFormId_ = 0x00067181;
		fragmentCounts_.fill(1);

		tierSettings_[static_cast<std::size_t>(SpellTier::Novice)] = { 3.0f, 10 };
		tierSettings_[static_cast<std::size_t>(SpellTier::Apprentice)] = { 6.0f, 8 };
		tierSettings_[static_cast<std::size_t>(SpellTier::Adept)] = { 12.0f, 6 };
		tierSettings_[static_cast<std::size_t>(SpellTier::Expert)] = { 20.0f, 4 };
		tierSettings_[static_cast<std::size_t>(SpellTier::Master)] = { 30.0f, 3 };
	}

	// Returns the singleton config instance.
	Config& Config::GetSingleton()
	{
		static Config instance;
		return instance;
	}

	// Loads configuration from the INI file, falling back to defaults.
	void Config::Load()
	{
		const auto path = GetConfigPath();
		std::ifstream file(path);
		if (!file.is_open()) {
			logger::info("Config file not found, using defaults: {}", path.string());
			Save();
			return;
		}

		logger::info("Loading config from {}", path.string());

		std::string currentSection;
		std::string line;
		bool sawStoreKey = false;
		bool sawFiniteUse = false;
		bool sawShowUsesRemaining = false;
		bool sawRequireFilledSoulGem = false;
		bool sawMaxStoredGems = false;
		std::array<bool, 4> sawSlotKey{};
		std::array<bool, static_cast<std::size_t>(SpellTier::Total)> sawCooldown{};
		std::array<bool, static_cast<std::size_t>(SpellTier::Total)> sawUses{};
		while (std::getline(file, line)) {
			line = Trim(line);
			if (line.empty() || line.front() == ';' || line.front() == '#') {
				continue;
			}

			if (line.front() == '[' && line.back() == ']') {
				currentSection = Trim(line.substr(1, line.size() - 2));
				continue;
			}

			auto delimiter = line.find('=');
			if (delimiter == std::string::npos) {
				continue;
			}

			auto key = Trim(line.substr(0, delimiter));
			auto value = Trim(line.substr(delimiter + 1));

			if (currentSection == "Settings" && key == "StarCooldown") {
				starCooldown_ = std::stof(value);
				starCooldown_ = starCooldown_ < 0.0f ? 0.0f : starCooldown_;
				logger::info("Config StarCooldown = {}", starCooldown_);
				continue;
			}

			if (currentSection == "Input" && key == "StoreKey") {
				storeKey_ = static_cast<std::uint32_t>(std::stoul(value));
				logger::info("Config StoreKey = {}", storeKey_);
				sawStoreKey = true;
				continue;
			}
			if (currentSection == "Activation" && key == "MaxStoredGems") {
				SetMaxStoredGems(static_cast<std::uint8_t>(std::stoul(value)));
				logger::info("Config MaxStoredGems = {}", maxStoredGems_);
				sawMaxStoredGems = true;
				continue;
			}
			if (currentSection == "Activation" && key.starts_with("Slot") && key.ends_with("Key")) {
				auto indexText = key.substr(4, key.size() - 7);
				std::size_t index = static_cast<std::size_t>(std::stoul(indexText));
				if (index > 0 && index <= activationKeys_.size()) {
					activationKeys_[index - 1] = static_cast<std::uint32_t>(std::stoul(value));
					logger::info("Config Slot{}Key = {}", index, activationKeys_[index - 1]);
					sawSlotKey[index - 1] = true;
				}
				continue;
			}
			if (currentSection == "Settings" && key == "FiniteUse") {
				bool parsed = finiteUse_;
				if (TryParseBool(value, parsed)) {
					finiteUse_ = parsed;
					logger::info("Config FiniteUse = {}", finiteUse_);
					sawFiniteUse = true;
				}
				continue;
			}
			if (currentSection == "Settings" && key == "RequireFilledSoulGem") {
				bool parsed = requireFilledSoulGem_;
				if (TryParseBool(value, parsed)) {
					requireFilledSoulGem_ = parsed;
					logger::info("Config RequireFilledSoulGem = {}", requireFilledSoulGem_);
					sawRequireFilledSoulGem = true;
				}
				continue;
			}
			if (currentSection == "Settings" && key == "AllowAnyGemTier") {
				bool parsed = allowAnyGemTier_;
				if (TryParseBool(value, parsed)) {
					allowAnyGemTier_ = parsed;
					logger::info("Config AllowAnyGemTier = {}", allowAnyGemTier_);
				}
				continue;
			}
			if (currentSection == "Settings" && key == "BlackSoulGemBoosts") {
				bool parsed = blackSoulGemBoosts_;
				if (TryParseBool(value, parsed)) {
					blackSoulGemBoosts_ = parsed;
					logger::info("Config BlackSoulGemBoosts = {}", blackSoulGemBoosts_);
				}
				continue;
			}
			if (currentSection == "Settings" && key == "NormalGemPenalty") {
				bool parsed = normalGemPenalty_;
				if (TryParseBool(value, parsed)) {
					normalGemPenalty_ = parsed;
					logger::info("Config NormalGemPenalty = {}", normalGemPenalty_);
				}
				continue;
			}
			if (currentSection == "Settings" && key == "AzurasStarBoost") {
				bool parsed = azurasStarBoost_;
				if (TryParseBool(value, parsed)) {
					azurasStarBoost_ = parsed;
					logger::info("Config AzurasStarBoost = {}", azurasStarBoost_);
				}
				continue;
			}
			if (currentSection == "Settings" && key == "ShowUsesRemaining") {
				bool parsed = showUsesRemaining_;
				if (TryParseBool(value, parsed)) {
					showUsesRemaining_ = parsed;
					logger::info("Config ShowUsesRemaining = {}", showUsesRemaining_);
					sawShowUsesRemaining = true;
				}
				continue;
			}

			for (std::size_t i = 0; i < tierSettings_.size(); ++i) {
				const auto* tierName = kTierNames[i];
				if (currentSection != tierName) {
					continue;
				}

				auto& settings = tierSettings_[i];
			if (key == "Cooldown") {
					settings.cooldown = std::stof(value);
					logger::info("Config {} Cooldown = {}", tierName, settings.cooldown);
					sawCooldown[i] = true;
				} else if (key == "Uses") {
					settings.uses = std::stoi(value);
					logger::info("Config {} Uses = {}", tierName, settings.uses);
					sawUses[i] = true;
			} else if (key == "FragmentCount") {
				fragmentCounts_[i] = static_cast<std::uint32_t>(std::stoul(value));
				logger::info("Config {} FragmentCount = {}", tierName, fragmentCounts_[i]);
				}
			}
		}

		const bool missingActivationKeys = std::any_of(sawSlotKey.begin(), sawSlotKey.end(), [](bool value) { return !value; });
		const bool missingCooldowns = std::any_of(sawCooldown.begin(), sawCooldown.end(), [](bool value) { return !value; });
		const bool missingUses = std::any_of(sawUses.begin(), sawUses.end(), [](bool value) { return !value; });
		const bool needsSave = !sawStoreKey || !sawFiniteUse || !sawShowUsesRemaining || !sawRequireFilledSoulGem ||
			!sawMaxStoredGems || missingActivationKeys || missingCooldowns || missingUses;
		if (needsSave) {
			logger::info("Config missing entries; writing defaults to {}", path.string());
			Save();
		}
	}

	// Writes the current configuration to the INI file.
	void Config::Save() const
	{
		const auto path = GetConfigPath();
		std::filesystem::create_directories(path.parent_path());

		std::ofstream file(path, std::ios::trunc);
		if (!file.is_open()) {
			logger::info("Failed to write config to {}", path.string());
			return;
		}

		file << "[Input]\n";
		file << "StoreKey=" << storeKey_ << "\n\n";

		file << "[Settings]\n";
		file << "FiniteUse=" << (finiteUse_ ? "true" : "false") << "\n";
		file << "RequireFilledSoulGem=" << (requireFilledSoulGem_ ? "true" : "false") << "\n";
		file << "AllowAnyGemTier=" << (allowAnyGemTier_ ? "true" : "false") << "\n";
		file << "BlackSoulGemBoosts=" << (blackSoulGemBoosts_ ? "true" : "false") << "\n";
		file << "NormalGemPenalty=" << (normalGemPenalty_ ? "true" : "false") << "\n";
		file << "AzurasStarBoost=" << (azurasStarBoost_ ? "true" : "false") << "\n";
		file << "FocusSpellDuration=" << focusSpellDuration_ << "\n";
		file << "StarCooldown=" << starCooldown_ << "\n";
		{
			std::ostringstream fragmentStream;
			fragmentStream << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << fragmentFormId_;
			file << "FragmentFormID=" << fragmentStream.str() << "\n";
		}
		file << "ShowUsesRemaining=" << (showUsesRemaining_ ? "true" : "false") << "\n\n";

		file << "[Activation]\n";
		file << "MaxStoredGems=" << static_cast<int>(maxStoredGems_) << "\n";
		for (std::size_t i = 0; i < activationKeys_.size(); ++i) {
			file << "Slot" << (i + 1) << "Key=" << activationKeys_[i] << "\n";
		}
		file << "\n";

		for (std::size_t i = 0; i < tierSettings_.size(); ++i) {
			const auto* tierName = kTierNames[i];
			const auto& settings = tierSettings_[i];
			file << '[' << tierName << "]\n";
			file << "Cooldown=" << settings.cooldown << "\n";
			file << "Uses=" << settings.uses << "\n";
			file << "FragmentCount=" << fragmentCounts_[i] << "\n\n";
		}

		logger::info("Config saved to {}", path.string());
	}

	const TierSettings& Config::GetTierSettings(SpellTier tier) const
	{
		return tierSettings_[static_cast<std::size_t>(tier)];
	}

	TierSettings& Config::GetTierSettings(SpellTier tier)
	{
		return tierSettings_[static_cast<std::size_t>(tier)];
	}

	std::uint32_t Config::GetStoreKey() const
	{
		return storeKey_;
	}

	void Config::SetStoreKey(std::uint32_t key)
	{
		storeKey_ = key;
	}

	std::uint32_t Config::GetActivationKey(std::size_t index) const
	{
		if (index >= activationKeys_.size()) {
			return 0;
		}
		return activationKeys_[index];
	}

	void Config::SetActivationKey(std::size_t index, std::uint32_t key)
	{
		if (index >= activationKeys_.size()) {
			return;
		}
		activationKeys_[index] = key;
	}

	std::uint8_t Config::GetMaxStoredGems() const
	{
		return maxStoredGems_;
	}

	void Config::SetMaxStoredGems(std::uint8_t value)
	{
		maxStoredGems_ = std::clamp<std::uint8_t>(value, 3, 5);
	}

	bool Config::IsFiniteUse() const
	{
		return finiteUse_;
	}

	void Config::SetFiniteUse(bool value)
	{
		finiteUse_ = value;
	}

	bool Config::ShowUsesRemaining() const
	{
		return showUsesRemaining_;
	}

	void Config::SetShowUsesRemaining(bool value)
	{
		showUsesRemaining_ = value;
	}

	bool Config::RequireFilledSoulGem() const
	{
		return requireFilledSoulGem_;
	}

	void Config::SetRequireFilledSoulGem(bool value)
	{
		requireFilledSoulGem_ = value;
	}

	bool Config::AllowAnyGemTier() const
	{
		return allowAnyGemTier_;
	}

	void Config::SetAllowAnyGemTier(bool value)
	{
		allowAnyGemTier_ = value;
	}

	bool Config::BlackSoulGemBoosts() const
	{
		return blackSoulGemBoosts_;
	}

	void Config::SetBlackSoulGemBoosts(bool value)
	{
		blackSoulGemBoosts_ = value;
	}

	bool Config::NormalGemPenalty() const
	{
		return normalGemPenalty_;
	}

	void Config::SetNormalGemPenalty(bool value)
	{
		normalGemPenalty_ = value;
	}

	bool Config::AzurasStarBoost() const
	{
		return azurasStarBoost_;
	}

	void Config::SetAzurasStarBoost(bool value)
	{
		azurasStarBoost_ = value;
	}

	float Config::GetFocusSpellDuration() const
	{
		return focusSpellDuration_;
	}

	void Config::SetFocusSpellDuration(float value)
	{
		focusSpellDuration_ = std::clamp(value, 0.0f, 3.0f);
	}

	float Config::GetStarCooldown() const
	{
		return starCooldown_;
	}

	void Config::SetStarCooldown(float value)
	{
		starCooldown_ = value < 0.0f ? 0.0f : value;
	}

	std::uint32_t Config::GetFragmentFormId() const
	{
		return fragmentFormId_;
	}

	void Config::SetFragmentFormId(std::uint32_t value)
	{
		fragmentFormId_ = value;
	}

	std::uint32_t Config::GetFragmentCount(SpellTier tier) const
	{
		return fragmentCounts_[static_cast<std::size_t>(tier)];
	}

	void Config::SetFragmentCount(SpellTier tier, std::uint32_t value)
	{
		fragmentCounts_[static_cast<std::size_t>(tier)] = value;
	}

	std::string_view Config::GetTierName(SpellTier tier)
	{
		return kTierNames[static_cast<std::size_t>(tier)];
	}
}
