#include "SystemMenu/HealthAndSafetyScreen.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuClock.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ScreenManager.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "health";
constexpr std::string_view LayoutName = "it_Has_a";

// The layout holds the warning and the notice of every area and language, each as a picture below one of these two panes.
constexpr std::string_view WarningPictures = "N_Has";
constexpr std::string_view NoticePictures = "N_Push";

// WeeOS speaks English and lives in the USA for now (docs/audit/OpenPoints/SystemMenu.md, the language pane).
constexpr std::string_view EnglishWarningPicture = "Has_US_ENG";
constexpr std::string_view EnglishNoticePicture = "Push_US_ENG";

constexpr std::string_view FadeInAnimation = "it_Has_a_SeenIn";
constexpr std::string_view NoticeAnimation = "it_Has_a_Push";
constexpr std::string_view FadeOutAnimation = "it_Has_a_SeenOut";

// The groups of the layout that the animations play on. The notice has its own group, so it is not part of the fade of the warning.
constexpr std::string_view WarningGroup = "G_All";
constexpr std::string_view NoticeGroup = "G_Push";

// The notice starts this long after the warning has faded in.
constexpr double SecondsBeforeNotice = 1;

}

HealthAndSafetyScreen::HealthAndSafetyScreen(MenuServices& menuServices)
    : Screen(ScreenId::HealthAndSafety)
    , services(menuServices)
    , continueWatcher(menuServices.input, menuServices.clock)
{
}

void HealthAndSafetyScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
}

void HealthAndSafetyScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName), PicturesOfTheScreen());
    ShowOnlyTheEnglishWarning();

    seenInSlot = layout->BindAnimation(std::string(FadeInAnimation), WarningGroup, Layout::PlaybackMode::PlayOnce);
    noticeSlot = layout->BindAnimation(std::string(NoticeAnimation), NoticeGroup, Layout::PlaybackMode::Loop);
    seenOutSlot = layout->BindAnimation(std::string(FadeOutAnimation), WarningGroup, Layout::PlaybackMode::PlayOnce);
    layout->StartAnimation(seenInSlot);
    layout->Calculate();

    services.fader.SetClear();
    services.fader.Calculate();
    services.cursors.SetVisible(false);
}

const Layout::PictureSource& HealthAndSafetyScreen::PicturesOfTheScreen() const
{
    return *archive;
}

void HealthAndSafetyScreen::ShowOnlyTheEnglishWarning()
{
    for (Formats::Pane& picture : layout->PaneNamed(WarningPictures).children)
        picture.isVisible = false;

    for (Formats::Pane& picture : layout->PaneNamed(NoticePictures).children)
        picture.isVisible = false;

    layout->PaneNamed(EnglishWarningPicture).isVisible = true;
    noticePicture = &layout->PaneNamed(EnglishNoticePicture);
}

void HealthAndSafetyScreen::Draw(DrawPass pass)
{
    if (pass == DrawPass::Main)
        layout->Draw();
}

bool HealthAndSafetyScreen::DrawsWithChildren() const
{
    return true;
}

bool HealthAndSafetyScreen::IsReady()
{
    NoteWhenTheWarningHasFadedIn();

    // The original also waits here until the fonts, the sounds and the other shared files have arrived. WeeOS has none to wait for yet.
    bool hasWaitedForTheNotice = hasWarningFadedIn && services.clock.Seconds() - noticeWaitBeganSeconds > SecondsBeforeNotice;
    if (hasWaitedForTheNotice)
        StartNotice();

    layout->Calculate();
    return hasWaitedForTheNotice;
}

void HealthAndSafetyScreen::NoteWhenTheWarningHasFadedIn()
{
    if (hasWarningFadedIn || layout->IsAnimationPlaying(seenInSlot))
        return;

    hasWarningFadedIn = true;
    noticeWaitBeganSeconds = services.clock.Seconds();
}

void HealthAndSafetyScreen::StartNotice()
{
    noticePicture->isVisible = true;
    layout->StartAnimation(noticeSlot);
    continueWatcher.Begin();
}

bool HealthAndSafetyScreen::RunMain()
{
    bool isDone = continueWatcher.HasUserConfirmed();
    if (isDone)
        layout->StartAnimation(seenOutSlot);

    layout->Calculate();
    return isDone;
}

bool HealthAndSafetyScreen::RunClosing()
{
    layout->Calculate();

    if (!layout->IsAnimationPlaying(seenOutSlot))
        services.fader.StartFadeOut();

    if (services.fader.GetStatus() != FaderStatus::Covered)
        return false;

    services.cursors.SetVisible(true);
    GoToRoot(ScreenId::MainFrame, 0);
    return true;
}

}
