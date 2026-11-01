#pragma once

#include "SystemMenu/ButtonGroup.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/WiiMenu/ChannelLaunch.h"
#include "SystemMenu/WiiMenu/ChannelManager.h"

#include "Formats/BannerContent.h"
#include "Layout/LayoutArchive.h"

#include <memory>
#include <optional>
#include <string>

namespace SystemMenu
{

class ChannelGridScreen;

// What the channel start window is doing. The numbers of the original's states are in the comments.
enum class StartWindowState
{
    // The window is built and asks the grid to zoom into the television of the channel (2).
    WaitingForTheZoom,

    // The grid zooms in; the window fades in inside the television (3).
    ZoomingIn,

    // The banner plays, and the hands can press Wii Menu and Start (1).
    ShowingChannel,

    // The menu's message that the channel cannot be used is open, because WeeOS has no program for it yet.
    ShowingCannotBeUsed,

    // The question whether to set up the Internet first is open (0x0C).
    AskingForInternetSettings,

    // Start was pressed and the channel may start: a moment passes, then the picture fades to black (4, 5).
    Starting,
    FadingOutToStart,

    // The title was handed to the console, which starts it after this step.
    Started,

    // Wii Settings comes next, for the Internet settings: the picture fades out first (7).
    LeavingForSettings,

    // Wii Menu was pressed: the grid zooms out, the window fades out inside the television (6).
    ZoomingOut,
};

// The window that opens when a channel of the grid is pressed (screen 0x03, chanTtl.ash, docs/research/SystemMenuStructure.md 4.5):
// the banner of the channel with its sound, or the Disc Channel without a disc, and the buttons Wii Menu and Start. Start runs the
// launch of the channel: the play record is written, the picture fades to black and the console starts the title.
class ChannelStartWindowScreen : public Screen
{
public:
    explicit ChannelStartWindowScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    // The window stays in view, and goes on, under the password dialog (the mode word 3 of the original).
    bool UpdatesWithChildren() const override;
    bool DrawsWithChildren() const override;

    StartWindowState State() const;

    // False when Start is gray: the Disc Channel without a disc.
    bool CanStart() const;

    // The words of the Start button: "Start", or "Update" for a channel whose version is too old (SYS-04).
    const std::u16string& StartButtonText() const;

protected:
    void BeforeStep() override;
    bool IsReady() override;
    void OnStart() override;
    bool RunMain() override;

private:
    void ReadTheChannel();
    void ReadTheBanner();
    void BuildTheFrame();
    void BuildTheBanner();
    void BuildTheDiscBanner();
    void StartTheBanner();

    void RunShowingChannel();
    void ReactToButton(const ButtonGroupEvent& event);
    void PressStart();
    void ShowMessage(std::uint16_t messageNumber);
    void AskForInternetSettings();
    void ReadAnswerOfCannotBeUsed();
    void ReadAnswerOfInternetQuestion();
    void BeginStart();
    void RunStarting();
    void RunFadingOutToStart();
    void LeaveForSettings(int settingsPage);
    void CloseTowardsTheGrid();

    ChannelGridScreen* Grid();
    void DrawTheWindow();
    void DrawTheWindowInsideTheTelevision();

    MenuServices& services;
    SlotPosition slot;
    bool isDiscChannel = false;

    // The title of the channel, its banner and what the menu knows about it.
    Ios::TitleId titleId;
    std::optional<Formats::BannerContent> banner;
    BannerAnimations bannerAnimations;
    std::u16string startButtonText;
    std::uint16_t makerCode = 0;

    std::unique_ptr<Layout::LayoutArchive> windowArchive;
    std::unique_ptr<Layout::LayoutArchive> discArchive;
    std::unique_ptr<Layout::LayoutArchive> bannerArchive;
    std::unique_ptr<MenuLayout> frame;
    std::unique_ptr<MenuLayout> bannerLayout;
    std::unique_ptr<ButtonGroup> buttons;

    // The animations of the buttons, by the slots of the frame: the focus comes and goes, a press, Start in gray.
    std::size_t focusOnOfWiiMenu = 0;
    std::size_t focusOffOfWiiMenu = 0;
    std::size_t focusOnOfStart = 0;
    std::size_t focusOffOfStart = 0;
    std::size_t pressOfWiiMenu = 0;
    std::size_t pressOfStart = 0;

    // The banner plays its start once and then its loop.
    std::optional<std::size_t> bannerStart;
    std::optional<std::size_t> bannerLoop;
    bool isBannerLooping = false;

    StartWindowState state = StartWindowState::WaitingForTheZoom;
    int stepsWaited = 0;
    int settingsPageToOpen = 0;
};

}
