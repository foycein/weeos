#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The names of the sound effects that the screens of Wii Options ask for, as they are named in the sound archive IplSound.brsar
// (docs/research/SystemMenuStructure.md 6.5).
namespace SoundName
{

constexpr std::string_view ButtonTargeted = "WIPL_SE_BT_TARGETTING";
constexpr std::string_view Decide = "WIPL_SE_DECIDE";
constexpr std::string_view Cancel = "WIPL_SE_CANCEL";

}

// The sounds that the screens want to hear in one step of the menu. The menu has no sound player yet: the player will take
// the names from here after every step and play them.
class SoundRequests
{
public:
    void Request(std::string_view soundName)
    {
        names.emplace_back(soundName);
    }

    // The sounds requested since the last call of StartNextStep, in the order in which they were requested.
    const std::vector<std::string>& RequestedInThisStep() const
    {
        return names;
    }

    void StartNextStep()
    {
        names.clear();
    }

private:
    std::vector<std::string> names;
};

}
