#pragma once

#include <string_view>
#include "REL/Version.h"

// Plugin metadata constants.
namespace Plugin
{
    inline constexpr std::string_view NAME = "Spell Gems";
    inline constexpr REL::Version VERSION{ 1, 0, 7, 0 };
}
