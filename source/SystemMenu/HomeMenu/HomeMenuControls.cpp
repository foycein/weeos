#include "SystemMenu/HomeMenu/HomeButtonMenu.h"

#include "SystemMenu/HomeMenu/HomeMenuNames.h"

#include "Layout/PaneRectangle.h"
#include "Layout/TransformMatrix.h"

#include <algorithm>

// The pointer and the buttons in the HOME menu: which place a pointer is on (the pointer areas of 0x81375034), what a pointer does when
// it reaches or leaves a place (0x81375300 and 0x813757B8), and what the A button does there (0x81375B78). HOME, + and - (0x81374894).

namespace SystemMenu
{

namespace
{

// The panes that the pointer can point at. The picture of the remote counts as the bar of the remote settings.
// The numbers are the places in the enumeration Area of the menu, in its order.
constexpr std::array<std::string_view, 11> AreaPaneNames = {
    "B_btnL_00", "B_btn_00", "B_bar_10", "B_optnBtn_00", "B_optnBtn_01", "B_optnBtn_10", "B_optnBtn_11", "B_optnBtn_20", "B_BtnA", "B_BtnB",
    "cntrl_00"};
constexpr std::size_t PictureOfTheRemote = 10;
constexpr std::size_t RemoteSettingsBarPane = 2;

bool WasPressed(const Input::RemoteState& state, Input::DeviceKind kind, Input::ButtonMask remoteButton, Input::ButtonMask classicButton)
{
    if ((state.buttonsPressed & remoteButton) != 0)
        return true;

    return kind == Input::DeviceKind::RemoteWithClassicController && (state.classic.buttonsPressed & classicButton) != 0;
}

bool ContainsPoint(const Layout::PanePlacement& placement, Formats::Vector2 point)
{
    std::optional<Formats::Vector2> pointInPane = Layout::FindPointMovedTo(placement.matrix, point);
    if (!pointInPane)
        return false;

    return Layout::Contains(Layout::RectangleOfPane(*placement.pane), *pointInPane);
}

}

void HomeButtonMenu::HandleButtonsOfPlayer(int player, const Input::RemoteState& state, Input::DeviceKind kind)
{
    MovePointer(player, AreaAt(state));

    std::optional<Area> pointedArea = players[player].pointedArea;
    if (pointedArea && WasPressed(state, kind, Input::RemoteButtons::A, Input::ClassicButtons::A))
        Press(*pointedArea);

    if (step != Step::Ready)
        return;

    if (WasPressed(state, kind, Input::RemoteButtons::Home, Input::ClassicButtons::Home))
    {
        if (part == Part::RemoteSettings)
            CloseRemoteSettings();
        else if (part == Part::Menu)
            PressClose();
        return;
    }

    if (part != Part::RemoteSettings)
        return;

    if (WasPressed(state, kind, Input::RemoteButtons::Minus, Input::ClassicButtons::Minus))
        PressVolumeDown();
    else if (WasPressed(state, kind, Input::RemoteButtons::Plus, Input::ClassicButtons::Plus))
        PressVolumeUp();
}

// The front most pane under the pointer that is a place of the part that is on.
std::optional<HomeButtonMenu::Area> HomeButtonMenu::AreaAt(const Input::RemoteState& state) const
{
    if (!state.pointerIsValid)
        return std::nullopt;

    Formats::Vector2 point = display.PointerToCanvas(state.pointer);
    std::vector<Layout::PanePlacement> placements = Layout::PlaceShownPanes(layout.RootPane(), placementSettings);
    for (auto placement = placements.rbegin(); placement != placements.rend(); ++placement)
    {
        auto paneName = std::find(AreaPaneNames.begin(), AreaPaneNames.end(), placement->pane->name);
        if (paneName == AreaPaneNames.end())
            continue;

        std::size_t index = static_cast<std::size_t>(paneName - AreaPaneNames.begin());
        if (index == PictureOfTheRemote)
            index = RemoteSettingsBarPane;

        Area area = static_cast<Area>(index);
        if (IsActive(area) && ContainsPoint(*placement, point))
            return area;
    }
    return std::nullopt;
}

bool HomeButtonMenu::IsActive(Area area) const
{
    switch (part)
    {
    case Part::Menu:
        return area == Area::BigButton || area == Area::Close || area == Area::RemoteSettingsBar;

    case Part::RemoteSettings:
        return area == Area::RemoteSettingsBar || area == Area::VolumeDown || area == Area::VolumeUp || area == Area::RumbleOn
            || area == Area::RumbleOff || area == Area::Reconnect;

    case Part::Question:
        return area == Area::Yes || area == Area::No;
    }
    return false;
}

void HomeButtonMenu::MovePointer(int player, std::optional<Area> newArea)
{
    std::optional<Area>& pointedArea = players[player].pointedArea;
    if (pointedArea == newArea)
        return;

    if (pointedArea)
        LeaveArea(*pointedArea);

    pointedArea = newArea;
    if (newArea)
        EnterArea(player, *newArea);
}

// The first pointer on a place lights it up with a sound, and its remote rumbles briefly.
void HomeButtonMenu::EnterArea(int player, Area area)
{
    bool hasLitUp = false;
    if (step == Step::Ready && pointersOnArea[area] == 0)
        hasLitUp = StartFocus(area);

    ++pointersOnArea[area];

    PlayerRecord& record = players[player];
    if (!hasLitUp || record.isRumbling || record.rumblePauseStepsLeft > 0)
        return;

    record.focusRumbleStepsLeft = FocusRumbleSteps;
    StartRumble(player);
}

bool HomeButtonMenu::StartFocus(Area area)
{
    switch (area)
    {
    case Area::BigButton:
        Start("_cntBtn_in", "btnL_00_inOut");
        break;

    case Area::Close:
        if (!AreTopBarAnimationsIdle())
        {
            pendingTopBarAnimation = AnimationName{"_hmMenu_bar_in", "hmMenu_bar_in"};
            return false;
        }
        RequestTopBarAnimation({"_hmMenu_bar_in", "hmMenu_bar_in"});
        break;

    case Area::RemoteSettingsBar:
        if (!AreBottomBarAnimationsIdle())
        {
            pendingBottomBarAnimation = BottomBarFocusAnimation(true);
            return false;
        }
        RequestBottomBarAnimation(BottomBarFocusAnimation(true));
        break;

    case Area::Yes:
        Start("_cmn_msg_btn_in", "msgBtn_00_inOut");
        break;

    case Area::No:
        Start("_cmn_msg_btn_in", "msgBtn_01_inOut");
        break;

    default:
        Start("_optn_btn_in", RemoteSettingsButtonOf(area) + "_inOut");
        break;
    }

    PlayFocusSound();
    return true;
}

// The last pointer that leaves a place lets it go dark again.
void HomeButtonMenu::LeaveArea(Area area)
{
    if (pointersOnArea[area] > 0)
        --pointersOnArea[area];

    if (step != Step::Ready || pointersOnArea[area] != 0)
        return;

    switch (area)
    {
    case Area::BigButton:
        if (part != Part::Question)
            Start("_cntBtn_out", "btnL_00_inOut");
        break;

    case Area::Close:
        if (part == Part::Menu)
            RequestTopBarAnimation({"_hmMenu_bar_out", "hmMenu_bar_out"});
        break;

    case Area::RemoteSettingsBar:
        if (part != Part::Question)
            RequestBottomBarAnimation(BottomBarFocusAnimation(false));
        break;

    case Area::Yes:
        if (part == Part::Question)
            Start("_cmn_msg_btn_out", "msgBtn_00_inOut");
        break;

    case Area::No:
        if (part == Part::Question)
            Start("_cmn_msg_btn_out", "msgBtn_01_inOut");
        break;

    default:
        if (part == Part::RemoteSettings)
            Start("_optn_btn_out", RemoteSettingsButtonOf(area) + "_inOut");
        break;
    }
}

void HomeButtonMenu::LeaveAllAreas()
{
    for (int player = 0; player < PlayerCount; ++player)
        MovePointer(player, std::nullopt);
}

// When another part of the menu comes up, the big button and the two bars let go of their pointers (0x813764D8).
void HomeButtonMenu::DropMenuFocus()
{
    if (pointersOnArea[Area::BigButton] != 0)
        Start("_cntBtn_out", "btnL_00_inOut");

    if (pointersOnArea[Area::Close] != 0)
        Start("_hmMenu_bar_out", "hmMenu_bar_out");

    bool isConnectWindowOpen = stepsInConnectWindow != 0 && part == Part::RemoteSettings;
    if (pointersOnArea[Area::RemoteSettingsBar] != 0 && !isConnectWindowOpen)
        Animation(BottomBarFocusAnimation(false)).Start();

    pointersOnArea[Area::BigButton] = 0;
    pointersOnArea[Area::Close] = 0;
    pointersOnArea[Area::RemoteSettingsBar] = 0;
}

void HomeButtonMenu::Press(Area area)
{
    if (step != Step::Ready)
        return;

    switch (area)
    {
    case Area::BigButton:
        PressBigButton();
        break;
    case Area::Close:
        PressClose();
        break;
    case Area::RemoteSettingsBar:
        if (part == Part::Menu)
            OpenRemoteSettings();
        else
            CloseRemoteSettings();
        break;
    case Area::VolumeDown:
        PressVolumeDown();
        break;
    case Area::VolumeUp:
        PressVolumeUp();
        break;
    case Area::RumbleOn:
        PressRumbleOn();
        break;
    case Area::RumbleOff:
        PressRumbleOff();
        break;
    case Area::Reconnect:
        PressReconnect();
        break;
    case Area::Yes:
        PressAnswer(true);
        break;
    case Area::No:
        PressAnswer(false);
        break;
    }
}

void HomeButtonMenu::PressBigButton()
{
    hasChosenButton = true;
    Play(HomeMenuSound::Select);

    if (!files.buttonAsks.front())
    {
        WaitFor("_cntBtn_psh", "btnL_00_psh", Step::AnswerShown);
        return;
    }

    part = Part::Question;
    SetVisible("T_Dialog", true);
    WaitFor("_cntBtn_psh", "btnL_00_psh", Step::QuestionOpening);
}

void HomeButtonMenu::PressClose()
{
    Stop("_hmMenu_bar_out", "hmMenu_bar_out");
    Stop("_hmMenu_bar_in", "hmMenu_bar_in");
    hasChosenButton = false;
    Play(HomeMenuSound::ReturnToProgram);
    WaitFor("_hmMenu_bar_psh", "hmMenu_bar_psh", Step::Closing);
}

void HomeButtonMenu::OpenRemoteSettings()
{
    Start("_optn_bar_psh", "optn_bar_psh");
    for (std::string_view paneName : HomeMenuNames::RemoteSettingsAreaPanes)
        SetVisible(std::string(paneName), true);

    part = Part::RemoteSettings;
    Play(HomeMenuSound::Select);
    WaitFor("_cntrl_up", "cntrl_up", Step::RemoteSettingsMoving);
}

// The original pushes the top bar too when the remote settings go down.
void HomeButtonMenu::CloseRemoteSettings()
{
    Start("_hmMenu_bar_psh", "hmMenu_bar_psh");
    Start("_cntrl_dwn", "cntrl_dwn");
    part = Part::Menu;
    Play(HomeMenuSound::CloseRemoteSettings);
    WaitFor("_close_bar_psh", "close_bar_psh", Step::RemoteSettingsMoving);
}

void HomeButtonMenu::PressVolumeDown()
{
    if (speakerVolumeStep == 0)
    {
        Play(HomeMenuSound::NothingDone);
        return;
    }

    ChangeVolume(speakerVolumeStep - 1, "optnBtn_00_psh", HomeMenuSound::VolumeDown, HomeMenuSound::VolumeDownToTheBottom);
}

void HomeButtonMenu::PressVolumeUp()
{
    if (speakerVolumeStep == HighestSpeakerVolumeStep)
    {
        Play(HomeMenuSound::NothingDone);
        return;
    }

    ChangeVolume(speakerVolumeStep + 1, "optnBtn_01_psh", HomeMenuSound::VolumeUp, HomeMenuSound::VolumeUpToTheTop);
}

// One bar changes its color, the speaker of every remote plays the sound of the volume at the new volume, and the button is pushed.
void HomeButtonMenu::ChangeVolume(int newStep, const std::string& pushedGroup, HomeMenuSound sound, HomeMenuSound soundAtTheEnd)
{
    bool isLouder = newStep > speakerVolumeStep;
    std::string group = HomeMenuNames::VolumeBarGroup(std::min(newStep, speakerVolumeStep));
    Stop(isLouder ? "_sound_gry" : "_sound_ylw", group);
    Start(isLouder ? "_sound_ylw" : "_sound_gry", group);
    speakerVolumeStep = newStep;

    bool isAtTheEnd = newStep == 0 || newStep == HighestSpeakerVolumeStep;
    Play(isAtTheEnd ? soundAtTheEnd : sound);

    for (int player = 0; player < PlayerCount; ++player)
        PlaySpeakerSound(player, files.volumeSound);

    Start("_optn_btn_psh", pushedGroup);
}

// While the button shows the choice, every remote rumbles, if the vibration is on.
void HomeButtonMenu::PressRumbleOn()
{
    if (isRumbleOn)
    {
        Play(HomeMenuSound::NothingDone);
        WaitFor("_vb_btn_ylw_ylw", "optnBtn_10_cntrl", Step::ShowingRumble);
    }
    else
    {
        isRumbleOn = true;
        remotes.SetRumbleEnabled(true);
        Start("_vb_btn_ylw_psh", "optnBtn_11_psh");
        Play(HomeMenuSound::RumbleOn);
        WaitFor("_vb_btn_wht_psh", "optnBtn_10_cntrl", Step::ShowingRumble);
    }

    for (int player = 0; player < PlayerCount; ++player)
    {
        StartRumble(player);
        players[player].focusRumbleStepsLeft = 0;
        players[player].rumblePauseStepsLeft = 0;
    }
}

void HomeButtonMenu::PressRumbleOff()
{
    if (!isRumbleOn)
    {
        Play(HomeMenuSound::NothingDone);
        waitedAnimation = nullptr;
        step = Step::ShowingRumble;
        return;
    }

    isRumbleOn = false;
    remotes.SetRumbleEnabled(false);
    Start("_vb_btn_wht_psh", "optnBtn_11_psh");
    Play(HomeMenuSound::RumbleOff);
    WaitFor("_vb_btn_ylw_psh", "optnBtn_10_psh", Step::ShowingRumble);
}

void HomeButtonMenu::PressReconnect()
{
    for (int player = 0; player < PlayerCount; ++player)
    {
        players[player].focusRumbleStepsLeft = 0;
        players[player].rumblePauseStepsLeft = 0;
        StopRumble(player);
    }

    stepsUntilDisconnect = StepsBeforeTheRemotesAreDisconnected;
    stepsInConnectWindow = 0;
    SetVisible("T_msg_00", true);
    SetVisible("T_msg_01", true);
    Play(HomeMenuSound::Select);
    Play(HomeMenuSound::ConnectWindowOpens);
    WaitFor("_optn_btn_psh", "optnBtn_20_psh", Step::ReconnectStarting);
}

void HomeButtonMenu::PressAnswer(bool isYes)
{
    part = Part::Menu;
    if (isYes)
    {
        Play(HomeMenuSound::GoToWiiMenu);
        WaitFor("_cmn_msg_btn_psh", "msgBtn_00_psh", Step::AnswerShown);
        return;
    }

    hasChosenButton = false;
    Play(HomeMenuSound::Cancel);
    WaitFor("_cmn_msg_btn_psh", "msgBtn_01_psh", Step::AnswerShown);
}

// The bar of the remote settings says "Wii Remote Settings" in the menu and "Close Wii Remote Settings" in the remote settings.
HomeButtonMenu::AnimationName HomeButtonMenu::BottomBarFocusAnimation(bool isEntering) const
{
    bool isInMenu = part == Part::Menu;
    if (isEntering)
        return {isInMenu ? "_optn_bar_in" : "_close_bar_in", "optn_bar_in"};

    return {isInMenu ? "_optn_bar_out" : "_close_bar_out", "optn_bar_out"};
}

void HomeButtonMenu::RequestTopBarAnimation(const AnimationName& name)
{
    if (!AreTopBarAnimationsIdle())
    {
        pendingTopBarAnimation = name;
        return;
    }

    Animation(name).Start();
    lastTopBarAnimation = name;
    pendingTopBarAnimation.reset();
}

void HomeButtonMenu::RequestBottomBarAnimation(const AnimationName& name)
{
    if (!AreBottomBarAnimationsIdle())
    {
        pendingBottomBarAnimation = name;
        return;
    }

    Animation(name).Start();
    lastBottomBarAnimation = name;
    pendingBottomBarAnimation.reset();
}

void HomeButtonMenu::StartPendingBarAnimations()
{
    if (pendingTopBarAnimation && AreTopBarAnimationsIdle())
    {
        if (pendingTopBarAnimation != lastTopBarAnimation)
        {
            Animation(*pendingTopBarAnimation).Start();
            lastTopBarAnimation = pendingTopBarAnimation;
        }
        pendingTopBarAnimation.reset();
    }

    if (pendingBottomBarAnimation && AreBottomBarAnimationsIdle())
    {
        if (pendingBottomBarAnimation != lastBottomBarAnimation)
        {
            Animation(*pendingBottomBarAnimation).Start();
            lastBottomBarAnimation = pendingBottomBarAnimation;
        }
        pendingBottomBarAnimation.reset();
    }
}

bool HomeButtonMenu::AreTopBarAnimationsIdle()
{
    if (step != Step::Ready)
        return false;

    return !IsPlaying("_hmMenu_bar_in", "hmMenu_bar_in") && !IsPlaying("_hmMenu_bar_out", "hmMenu_bar_out");
}

bool HomeButtonMenu::AreBottomBarAnimationsIdle()
{
    if (step != Step::Ready)
        return false;

    return !IsPlaying("_optn_bar_in", "optn_bar_in") && !IsPlaying("_close_bar_in", "optn_bar_in")
        && !IsPlaying("_optn_bar_out", "optn_bar_out") && !IsPlaying("_close_bar_out", "optn_bar_out");
}

std::string HomeButtonMenu::RemoteSettingsButtonOf(Area area)
{
    std::size_t button = static_cast<std::size_t>(area) - static_cast<std::size_t>(Area::VolumeDown);
    return std::string(HomeMenuNames::RemoteSettingsButtons.at(button));
}

// A focus sound is not played again within 3 steps.
void HomeButtonMenu::PlayFocusSound()
{
    if (stepsSinceFocusSound < StepsBetweenFocusSounds)
        return;

    Play(HomeMenuSound::Focus);
    stepsSinceFocusSound = 0;
}

}
