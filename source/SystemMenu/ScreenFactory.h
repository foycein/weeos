#pragma once

#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"

#include <memory>

namespace SystemMenu
{

// Makes the screen with this id (the factory of the original, 0x8140A008). Gives back nothing for the screens that are not built yet.
std::unique_ptr<Screen> MakeScreen(ScreenId screen, int argument, MenuServices& services);

}
