/*=============================================================================================================*/
//																											   //
//                                                  Spell Gems                                                 //
//                                              SKSE Initialization                                            //
//                                                                                                             //
/*=============================================================================================================*/


#include "SpellGems/Config.h"
#include "SpellGems/MenuUI.h"
#include "SpellGems/Serialization.h"
#include "SpellGems/SpellGemManager.h"
#include <keyhandler/keyhandler.h>

// Handles SKSE lifecycle messages to initialize plugin systems.
static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
    switch (message->type) {
    case SKSE::MessagingInterface::kDataLoaded: {
        logger::info("SpellGems data loaded message received.");
        auto& config = SpellGems::Config::GetSingleton();
        config.Load();

        SpellGems::MenuUI::Initialize();
        SpellGems::SpellGemManager::GetSingleton().RegisterUseEventSink();

        KeyHandler::RegisterSink();
        auto* keyHandler = KeyHandler::GetSingleton();
        const auto storeKey = config.GetStoreKey();

        [[maybe_unused]] auto storeHandler = keyHandler->Register(storeKey, KeyEventType::KEY_DOWN, []() {
            SpellGems::SpellGemManager::GetSingleton().TryStoreSelectedSpell();
        });

        SpellGems::SpellGemManager::GetSingleton().RegisterActivationKeys();

        logger::info("Store spell key registered: {}", storeKey);

        break;
    }
    }
}

// Entry point for loading the plugin via SKSE.
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
    logger::info("SpellGems plugin load start.");
    REL::Module::reset();

    auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));

    if (!g_messaging) {
        logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
        return false;
    }

    logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());

    SKSE::Init(a_skse);
    SKSE::AllocTrampoline(1 << 10);

    g_messaging->RegisterListener("SKSE", SKSEMessageHandler);
    logger::info("Registered SKSE message listener.");

    SpellGems::Serialization::GetSingleton().Initialize(SKSE::GetSerializationInterface());
    logger::info("Serialization callbacks registered.");

    return true;
}
