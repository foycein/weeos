#pragma once

#include "SystemMenu/ScreenId.h"

namespace SystemMenu
{

struct FirstScreen
{
    ScreenId screen;
    int argument;
};

// The screen that shows first. The original chooses by the reason the console started (docs/audit/OpenPoints/SystemMenu.md SYS-16):
// a console that is not set up opens Wii Settings, a channel that ran out of time shows "Limit over", a return from a channel shows
// "Back to Wii Menu", and a start with arguments goes through the start-up router. A cold start, with everything set up, shows Health and Safety.
// WeeOS starts cold only, and the other screens are not built, so the choice is this one.
FirstScreen ChooseFirstScreen();

}
