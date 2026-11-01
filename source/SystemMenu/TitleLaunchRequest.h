#pragma once

#include "Ios/Title/TitleId.h"

#include <optional>

namespace SystemMenu
{

// A program asks the console for another title, for example the menu for a channel when the user chooses Start in the
// channel's start window, or a channel for the Wii Menu. The program that runs ends after the step, IOS restarts with the IOS
// of the title and the title starts (Boot::ConsoleStartup::LaunchTitle). The last request of a step wins.
class TitleLaunchRequest
{
public:
    void Request(Ios::TitleId titleId)
    {
        requestedTitle = titleId;
    }

    // The title that was asked for, once. Nothing when no title was asked for since the last call.
    std::optional<Ios::TitleId> Take()
    {
        std::optional<Ios::TitleId> titleToLaunch = requestedTitle;
        requestedTitle.reset();
        return titleToLaunch;
    }

private:
    std::optional<Ios::TitleId> requestedTitle;
};

}
