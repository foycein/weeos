#include "SystemMenu/BackToWiiMenuScreen.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/ProgramImage.h"

#include "Formats/U8Archive.h"

namespace SystemMenu
{

namespace
{

// Where the archive of the layout lies in the memory image of the main program. It is a plain U8 archive.
constexpr std::uint32_t LayoutArchiveAddress = 0x816487E0;

constexpr std::string_view LayoutName = "my_BackToWiiMenu";
constexpr std::string_view AnimationName = "my_BackToWiiMenu";

}

BackToWiiMenuScreen::BackToWiiMenuScreen(MenuServices& menuServices)
    : Screen(ScreenId::BackToWiiMenu)
    , services(menuServices)
{
}

void BackToWiiMenuScreen::Load()
{
    Formats::U8Archive layoutFiles = Formats::ReadU8Archive(services.program.BytesAt(LayoutArchiveAddress));
    archive = std::make_unique<Layout::LayoutArchive>(std::move(layoutFiles));
}

void BackToWiiMenuScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName));

    std::size_t slot = layout->BindAnimationToAllPanes(std::string(AnimationName));
    layout->StartAnimation(slot);
    layout->Calculate();

    services.cursors.SetVisible(false);
    services.fader.StartFadeIn();
}

void BackToWiiMenuScreen::Draw(DrawPass pass)
{
    if (pass == DrawPass::Main)
        layout->Draw();
}

void BackToWiiMenuScreen::BeforeStep()
{
    layout->Calculate();
}

bool BackToWiiMenuScreen::RunMain()
{
    return services.fader.GetStatus() == FaderStatus::Clear;
}

void BackToWiiMenuScreen::OnRunEnded()
{
    services.fader.StartFadeOut();
}

bool BackToWiiMenuScreen::RunClosing()
{
    if (services.fader.GetStatus() != FaderStatus::Covered)
        return false;

    services.cursors.SetVisible(true);
    GoToRoot(ScreenId::MainFrame, 0);
    return true;
}

}
