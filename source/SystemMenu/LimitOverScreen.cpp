#include "SystemMenu/LimitOverScreen.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/MenuClock.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/NandFiles.h"
#include "SystemMenu/ScreenManager.h"

#include <array>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "limitOver";
constexpr std::string_view LayoutName = "my_Timer_c";
constexpr std::string_view ExpiredRecordPath = "/shared2/expired";

// The layout holds the message and the notice of every area and language, each as a picture below one of these two panes.
constexpr std::string_view MessagePictures = "N_Mes";
constexpr std::string_view NoticePictures = "N_Push";
constexpr std::string_view EnglishMessagePicture = "Mes_US_ENG";
constexpr std::string_view EnglishNoticePicture = "Push_US_ENG";

constexpr std::string_view FadeInAnimation = "my_Timer_c_Fade_In";
constexpr std::string_view NoticeAnimation = "my_Timer_c_Push_blink";
constexpr std::string_view FadeOutAnimation = "my_Timer_c_Fade_Out";
constexpr std::string_view MessageGroup = "G_All";
constexpr std::string_view NoticeGroup = "G_Push";

// The notice starts this long after the message has faded in.
constexpr double SecondsBeforeNotice = 1;

// The frame around the picture has pieces for a 4:3 and for a 16:9 screen. On a wide screen the pieces take the pictures of the
// three pieces that lie outside of the layout: a corner, a side and an edge.
struct FramePieceGroup
{
    std::string_view donorPane;
    std::array<std::string_view, 4> piecePanes;
};

constexpr std::array<FramePieceGroup, 3> WideFramePieceGroups = {{
    {"Picture_04", {"Fre_a", "Fre_d", "Fre_i", "Fre_l"}},
    {"Picture_05", {"Fre_e", "Fre_f", "Fre_g", "Fre_h"}},
    {"Picture_06", {"Fre_b", "Fre_c", "Fre_j", "Fre_k"}},
}};

}

LimitOverScreen::LimitOverScreen(MenuServices& menuServices)
    : Screen(ScreenId::LimitOver)
    , services(menuServices)
    , continueWatcher(menuServices.input, menuServices.clock)
{
}

void LimitOverScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
    services.nand.DeleteFile(ExpiredRecordPath);
}

void LimitOverScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName));
    if (services.display.GetScreenShape() == Platform::ScreenShape::SixteenByNine)
        FitFramePicturesToWideScreen();

    ShowOnlyTheEnglishMessage();

    fadeInSlot = layout->BindAnimation(std::string(FadeInAnimation), MessageGroup, Layout::PlaybackMode::PlayOnce);
    noticeSlot = layout->BindAnimation(std::string(NoticeAnimation), NoticeGroup, Layout::PlaybackMode::Loop);
    fadeOutSlot = layout->BindAnimation(std::string(FadeOutAnimation), MessageGroup, Layout::PlaybackMode::PlayOnce);

    services.cursors.SetVisible(false);
}

void LimitOverScreen::FitFramePicturesToWideScreen()
{
    for (const FramePieceGroup& group : WideFramePieceGroups)
    {
        const Formats::PaneContent& donor = *layout->PaneNamed(group.donorPane).picture;
        for (std::string_view pieceName : group.piecePanes)
            layout->PaneNamed(pieceName).picture->textureCoordinateSets = donor.textureCoordinateSets;
    }
}

void LimitOverScreen::ShowOnlyTheEnglishMessage()
{
    for (Formats::Pane& picture : layout->PaneNamed(MessagePictures).children)
        picture.isVisible = false;

    for (Formats::Pane& picture : layout->PaneNamed(NoticePictures).children)
        picture.isVisible = false;

    layout->PaneNamed(EnglishMessagePicture).isVisible = true;
    noticePicture = &layout->PaneNamed(EnglishNoticePicture);
}

void LimitOverScreen::Draw(DrawPass pass)
{
    if (pass == DrawPass::Main)
        layout->Draw();
}

// The original also waits for the sounds before it starts, and for the start-up of the menu before it shows the notice.
// WeeOS has nothing to wait for.
bool LimitOverScreen::IsReady()
{
    layout->Calculate();

    if (!hasFadeInStarted)
    {
        layout->StartAnimation(fadeInSlot);
        layout->Calculate();
        services.fader.SetClear();
        services.fader.Calculate();
        hasFadeInStarted = true;
        return false;
    }

    if (!hasMessageFadedIn && !layout->IsAnimationPlaying(fadeInSlot))
    {
        hasMessageFadedIn = true;
        noticeWaitBeganSeconds = services.clock.Seconds();
    }

    bool hasWaitedForTheNotice = hasMessageFadedIn && services.clock.Seconds() - noticeWaitBeganSeconds > SecondsBeforeNotice;
    if (!hasWaitedForTheNotice)
        return false;

    noticePicture->isVisible = true;
    layout->StartAnimation(noticeSlot);
    layout->Calculate();
    continueWatcher.Begin();
    return true;
}

bool LimitOverScreen::RunMain()
{
    bool isDone = continueWatcher.HasUserConfirmed();
    if (isDone)
        layout->StartAnimation(fadeOutSlot);

    layout->Calculate();
    return isDone;
}

// This screen calculates the fader itself as well, so its fade out runs twice as fast as the one of Health and Safety.
bool LimitOverScreen::RunClosing()
{
    layout->Calculate();

    if (!layout->IsAnimationPlaying(fadeOutSlot))
    {
        services.fader.StartFadeOut();
        services.fader.Calculate();
    }

    if (services.fader.GetStatus() != FaderStatus::Covered)
        return false;

    services.cursors.SetVisible(true);
    services.screens.ReplaceScreen(*this, ScreenId::MainFrame, 0);
    return true;
}

}
