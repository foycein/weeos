#include "SystemMenu/WiiOptionsBackgroundScreen.h"

#include "SystemMenu/Fader.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ScreenManager.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "setupBg";
constexpr std::string_view LayoutName = "it_BgSetUp_a";

}

WiiOptionsBackgroundScreen::WiiOptionsBackgroundScreen(MenuServices& menuServices, int screenArgument)
    : Screen(ScreenId::WiiOptionsBackground)
    , services(menuServices)
    , argument(screenArgument)
{
}

void WiiOptionsBackgroundScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
}

void WiiOptionsBackgroundScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName));
    layout->Calculate();

    services.screens.OpenChild(ScreenId::WiiOptionsBar, Id(), argument);
    services.screens.OpenChild(ScreenId::WiiOptionsChoices, Id(), argument);
    services.fader.StartFadeIn();
}

void WiiOptionsBackgroundScreen::Draw(DrawPass pass)
{
    if (pass == DrawPass::Main)
        layout->Draw();
}

bool WiiOptionsBackgroundScreen::UpdatesWithChildren() const
{
    return true;
}

bool WiiOptionsBackgroundScreen::DrawsWithChildren() const
{
    return true;
}

void WiiOptionsBackgroundScreen::AfterStep()
{
    layout->Calculate();
}

}
