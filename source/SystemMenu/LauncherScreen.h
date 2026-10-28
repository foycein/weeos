#pragma once

#include "SystemMenu/Screen.h"

namespace SystemMenu
{

class ScreenManager;

// The root of the tree of screens (screen 1). It has no look. When it is built it asks for the first screen as its child.
class LauncherScreen : public Screen
{
public:
    LauncherScreen(ScreenManager& screenManager, ScreenId firstScreenToOpen, int argumentOfFirstScreen);

    void Build() override;

private:
    ScreenManager& screens;
    ScreenId firstScreen;
    int firstScreenArgument;
};

}
