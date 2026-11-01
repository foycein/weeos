#include "SystemMenu/HomeMenu/HomeButtonMenu.h"

#include "SystemMenu/HomeMenu/HomeMenuNames.h"
#include "SystemMenu/MenuError.h"

#include "Layout/LayoutDrawer.h"

#include <format>
#include <set>

namespace SystemMenu
{

namespace
{

// The animations whose group has their own name (the list at 0x81644DE4).
const std::vector<std::string> AnimationsOfTheirOwnGroup = {"ltrIcn_on", "optn_bar_psh", "close_bar_psh", "hmMenu_bar_in", "hmMenu_bar_psh",
    "link_msg_in", "link_msg_out", "cmn_msg_in", "cmn_msg_out", "cntrl_up", "cntrl_wndw_opn", "cntrl_dwn", "hmMenu_bar_out", "cmn_msg_rtrn",
    "12btn_on"};

const std::vector<std::string> BatteryAnimations = {"_btry_wink", "_btry_gry", "_btry_wht", "_btry_wink_gry", "_btry_red"};

constexpr int ConnectWindowStepsLeftWhenFourAreConnected = 30;
constexpr int FadeOutSteps = 30;
constexpr int OpaqueAlpha = 255;
constexpr Formats::Rgba8Color Black = {0, 0, 0, 255};

// The first 22 sounds of IplSound.brsar, in the order of their numbers.
constexpr std::array<std::string_view, 22> SoundNames = {"HOMESE_HOME_BUTTON", "HOMESE_RETURN_APP", "HOMESE_GOTO_MENU",
    "HOMESE_RESET_APP", "HOMESE_FOCUS", "HOMESE_SELECT", "HOMESE_CANCEL", "HOMESE_OPEN_CONTROLLER", "HOMESE_CLOSE_CONTROLLER",
    "HOMESE_VOLUME_PLUS", "HOMESE_VOLUME_MINUS", "HOMESE_VOLUME_PLUS_LIMIT", "HOMESE_VOLUME_MINUS_LIMIT", "HOMESE_NOTHING_DONE",
    "HOMESE_VIBE_ON", "HOMESE_VIBE_OFF", "HOMESE_START_CONNECT_WINDOW", "HOMESE_CONNECTED", "HOMESE_CONNECTED2", "HOMESE_CONNECTED3",
    "HOMESE_CONNECTED4", "HOMESE_END_CONNECT_WINDOW"};

// The materials a pane draws with: the one of a picture or a text box, the content and the frames of a window.
std::vector<std::uint16_t> MaterialIndicesOf(const Formats::Pane& pane)
{
    std::vector<std::uint16_t> indices;
    if (pane.picture)
        indices.push_back(pane.picture->materialIndex);

    if (pane.textBox)
        indices.push_back(pane.textBox->materialIndex);

    if (pane.window)
    {
        indices.push_back(pane.window->content.materialIndex);
        for (const Formats::WindowFrame& frame : pane.window->frames)
            indices.push_back(frame.materialIndex);
    }
    return indices;
}

// The library of the HOME menu binds an animation to the panes of its group alone, not to the panes below them (0x81371BF4).
// The animation player of the layout binds the panes below too, so the animation keeps only what belongs to the panes of the group.
Formats::LayoutAnimation OnlyForThePanes(const Formats::LayoutAnimation& animation, const std::vector<Formats::Pane*>& panes,
    Layout::LiveLayout& layout)
{
    std::set<std::string> paneNames;
    std::set<std::string> materialNames;
    for (const Formats::Pane* pane : panes)
    {
        paneNames.insert(pane->name);
        for (std::uint16_t materialIndex : MaterialIndicesOf(*pane))
            materialNames.insert(layout.MaterialAt(materialIndex).description.name);
    }

    Formats::LayoutAnimation filtered = animation;
    filtered.objects.clear();
    for (const Formats::AnimatedObject& object : animation.objects)
    {
        const std::set<std::string>& names = object.isMaterial ? materialNames : paneNames;
        if (names.contains(object.name))
            filtered.objects.push_back(object);
    }
    return filtered;
}

}

std::string_view NameOfHomeMenuSound(HomeMenuSound sound)
{
    return SoundNames.at(static_cast<std::size_t>(sound));
}

HomeButtonMenu::HomeButtonMenu(Display& menuDisplay, const HomeMenuFiles& homeMenuFiles, const InputManager& inputManager,
    Ios::RemoteService& remoteService, CursorManager& cursorManager, const Layout::TextFonts* textFonts, HomeMenuSoundPlayer soundPlayer)
    : display(menuDisplay)
    , files(homeMenuFiles)
    , input(inputManager)
    , remotes(remoteService)
    , cursors(cursorManager)
    , fonts(textFonts)
    , playSound(std::move(soundPlayer))
    , layout(menuDisplay.Device(), *homeMenuFiles.archive, homeMenuFiles.archive->ReadLayout(homeMenuFiles.layoutName))
{
    if (files.buttonAsks.empty() || files.texts.buttonQuestions.empty())
        throw MenuError("The HOME menu has no big button");

    BindAllAnimations();
    SetText("T_msg_00", files.texts.reconnectInstruction);
    SetText("T_msg_01", files.texts.disconnecting);
    SetText("T_Dialog", files.texts.buttonQuestions.front());
}

// The start of the menu (0x813726B4 and the first state of 0x81372E9C).
void HomeButtonMenu::Open(int speakerVolume)
{
    step = Step::Opening;
    part = Part::Menu;
    outcome = HomeMenuOutcome::Open;
    speakerVolumeStep = speakerVolume;
    isRumbleOn = remotes.IsRumbleEnabled();
    hasChosenButton = false;
    fadeSteps = 0;
    pointersOnArea.clear();
    pendingTopBarAnimation.reset();
    pendingBottomBarAnimation.reset();
    lastTopBarAnimation.reset();
    lastBottomBarAnimation.reset();
    stepsUntilBatteryCheck = BatteryCheckIntervalSteps;

    SetVisible("let_icn_00", false);
    for (std::string_view paneName : HomeMenuNames::RemoteSettingsAreaPanes)
        SetVisible(std::string(paneName), false);

    SetVisible("T_msg_00", false);
    SetVisible("T_msg_01", false);
    SetVisible("T_Dialog", false);
    SetVisible("back_00", true);
    SetVisible("back_02", false);

    for (int player = 0; player < PlayerCount; ++player)
    {
        players[player] = PlayerRecord();
        players[player].isConnected = input.Player(player).has_value();
        std::string group = HomeMenuNames::PlayerGroup(player);
        Start(players[player].isConnected ? "_btry_wht" : "_btry_gry", group);
    }
    HideBatteries();
    for (int player = 0; player < PlayerCount; ++player)
    {
        if (players[player].isConnected)
            ShowBattery(player, input.Player(player)->state.batteryLevel);
    }

    WaitFor("_hmMenu_strt", "hmMenu_strt", Step::Opening);
    FitToScreenShape();
}

void HomeButtonMenu::Update()
{
    if (step == Step::Finished)
        return;

    StepAnimations();
    for (int player = 0; player < PlayerCount; ++player)
        UpdateRumble(player);

    RunStep();
    StartPendingBarAnimations();
    UpdateRemotes();
    FitToScreenShape();

    if (stepsSinceFocusSound < StepsBetweenFocusSounds)
        ++stepsSinceFocusSound;
}

void HomeButtonMenu::Draw()
{
    Layout::DrawLayout(display.Device(), layout, placementSettings, fonts);
    display.NoteDrawnLayout(layout, placementSettings);
    cursors.Draw();

    if (fadeSteps == 0)
        return;

    Graphics::Canvas canvas = display.GetCanvas();
    float halfWidth = canvas.width / 2;
    float halfHeight = canvas.height / 2;
    Formats::Rgba8Color color = Black;
    color.alpha = static_cast<std::uint8_t>(fadeSteps * OpaqueAlpha / FadeOutSteps);

    Graphics::Quad quad = {};
    quad.positions = {{{-halfWidth, halfHeight}, {halfWidth, halfHeight}, {halfWidth, -halfHeight}, {-halfWidth, -halfHeight}}};
    quad.colors = {{color, color, color, color}};
    display.Device().DrawQuad(quad, Graphics::Material());
}

HomeMenuOutcome HomeButtonMenu::Outcome() const
{
    return outcome;
}

int HomeButtonMenu::SpeakerVolumeStep() const
{
    return speakerVolumeStep;
}

// ---- Animations

// The order is the order of the three lists of the original, because two animations can change the same pane:
// the big buttons, the animations with a group of their own, then the list at 0x8160F3D8.
void HomeButtonMenu::BindAllAnimations()
{
    BindAnimation("_cntBtn_in", "btnL_00_inOut");
    BindAnimation("_cntBtn_out", "btnL_00_inOut");
    BindAnimation("_cntBtn_psh", "btnL_00_psh");

    for (const std::string& group : AnimationsOfTheirOwnGroup)
        BindAnimation("_" + group, group);

    BindAnimation("_hmMenu_strt", "hmMenu_strt");
    BindAnimation("_hmMenu_fnsh", "hmMenu_fnsh");
    BindAnimation("_optn_bar_in", "optn_bar_in");
    BindAnimation("_close_bar_in", "optn_bar_in");
    BindAnimation("_optn_bar_out", "optn_bar_out");
    BindAnimation("_close_bar_out", "optn_bar_out");

    for (std::string_view button : HomeMenuNames::RemoteSettingsButtons)
    {
        BindAnimation("_optn_btn_in", std::string(button) + "_inOut");
        BindAnimation("_optn_btn_out", std::string(button) + "_inOut");
    }

    BindAnimation("_optn_btn_psh", "optnBtn_00_psh");
    BindAnimation("_optn_btn_psh", "optnBtn_01_psh");
    BindAnimation("_vb_btn_wht_psh", "optnBtn_10_psh");
    BindAnimation("_vb_btn_ylw_psh", "optnBtn_10_psh");
    BindAnimation("_vb_btn_ylw_ylw", "optnBtn_11_psh");
    BindAnimation("_vb_btn_wht_psh", "optnBtn_11_psh");
    BindAnimation("_vb_btn_ylw_psh", "optnBtn_11_psh");
    BindAnimation("_optn_btn_psh", "optnBtn_20_psh");
    BindAnimation("_vb_btn_wht_psh", "optnBtn_10_cntrl");
    BindAnimation("_vb_btn_ylw_ylw", "optnBtn_10_cntrl");

    for (const std::string& button : {"msgBtn_00", "msgBtn_01"})
    {
        BindAnimation("_cmn_msg_btn_in", std::string(button) + "_inOut");
        BindAnimation("_cmn_msg_btn_out", std::string(button) + "_inOut");
    }
    BindAnimation("_cmn_msg_btn_psh", "msgBtn_00_psh");
    BindAnimation("_cmn_msg_btn_psh", "msgBtn_01_psh");

    for (int bar = 0; bar < HighestSpeakerVolumeStep; ++bar)
    {
        BindAnimation("_sound_gry", HomeMenuNames::VolumeBarGroup(bar));
        BindAnimation("_sound_ylw", HomeMenuNames::VolumeBarGroup(bar));
    }

    for (int player = 0; player < PlayerCount; ++player)
    {
        for (const std::string& suffix : BatteryAnimations)
            BindAnimation(suffix, HomeMenuNames::PlayerGroup(player));
    }
}

void HomeButtonMenu::BindAnimation(const std::string& suffix, const std::string& groupName)
{
    std::string fileName = files.animationPrefix + suffix;
    if (!animationFiles.contains(fileName))
        animationFiles.emplace(fileName, files.archive->ReadAnimation(fileName));

    std::vector<Formats::Pane*> panes = layout.PanesOfGroup(groupName);
    Formats::LayoutAnimation animation = OnlyForThePanes(animationFiles.at(fileName), panes, layout);
    animations.push_back(std::make_unique<HomeMenuAnimation>(layout, std::move(animation), panes));
    animationsByName[groupName + suffix] = animations.back().get();
}

HomeMenuAnimation& HomeButtonMenu::Animation(const AnimationName& name)
{
    return Animation(name.suffix, name.group);
}

HomeMenuAnimation& HomeButtonMenu::Animation(const std::string& suffix, const std::string& groupName)
{
    auto found = animationsByName.find(groupName + suffix);
    if (found == animationsByName.end())
        throw MenuError(std::format("The HOME menu has no animation {} for the group {}", suffix, groupName));

    return *found->second;
}

void HomeButtonMenu::Start(const std::string& suffix, const std::string& groupName)
{
    Animation(suffix, groupName).Start();
}

void HomeButtonMenu::Stop(const std::string& suffix, const std::string& groupName)
{
    Animation(suffix, groupName).Stop();
}

bool HomeButtonMenu::IsPlaying(const std::string& suffix, const std::string& groupName)
{
    return Animation(suffix, groupName).IsPlaying();
}

void HomeButtonMenu::StepAnimations()
{
    for (const std::unique_ptr<HomeMenuAnimation>& animation : animations)
        animation->Step();
}

// ---- Panes

Formats::Pane& HomeButtonMenu::PaneNamed(const std::string& paneName)
{
    Formats::Pane* pane = layout.FindPane(paneName);
    if (pane == nullptr)
        throw MenuError(std::format("The layout {} has no pane {}", files.layoutName, paneName));

    return *pane;
}

void HomeButtonMenu::SetVisible(const std::string& paneName, bool isVisible)
{
    PaneNamed(paneName).isVisible = isVisible;
}

void HomeButtonMenu::SetText(const std::string& paneName, const std::u16string& text)
{
    Formats::Pane& pane = PaneNamed(paneName);
    if (!pane.textBox)
        throw MenuError(std::format("The pane {} of the HOME menu is not a text box", paneName));

    pane.textBox->text = text;
}

// As every layout of the menu: stretched on a wide screen, with the panes that adjust their location squeezed back (SYS-17).
void HomeButtonMenu::FitToScreenShape()
{
    Formats::Vector2 stretch = display.LayoutStretch();
    layout.RootPane().scale = stretch;
    placementSettings.locationAdjustScale = {1 / stretch.x, 1 / stretch.y};
}

// ---- The steps

// The steps that run in every step of the menu, then the steps that only wait for their animation to end.
void HomeButtonMenu::RunStep()
{
    switch (step)
    {
    case Step::ReconnectStarting:
        RunReconnectStart();
        return;

    case Step::WaitingForDisconnect:
        if (input.ConnectedPlayers().none())
        {
            step = Step::ConnectWindow;
            stepsInConnectWindow = 0;
        }
        return;

    case Step::ConnectWindowClosing:
        FinishConnectWindow();
        return;

    case Step::FadingOut:
        ++fadeSteps;
        if (fadeSteps >= FadeOutSteps)
            Finish(HomeMenuOutcome::ButtonChosen);
        return;

    case Step::Ready:
    case Step::Finished:
        return;

    default:
        break;
    }

    bool isWaiting = waitedAnimation != nullptr && waitedAnimation->IsPlaying();
    if (!isWaiting)
        RunStepAfterAnimation();
}

void HomeButtonMenu::RunStepAfterAnimation()
{
    switch (step)
    {
    case Step::Opening:
        FinishOpening();
        break;

    case Step::RemoteSettingsMoving:
        FinishRemoteSettingsMove();
        break;

    case Step::WaitingForAnimation:
        FinishWaiting();
        break;

    // The remotes rumble as long as the button shows the choice.
    case Step::ShowingRumble:
        for (int player = 0; player < PlayerCount; ++player)
            StopRumble(player);
        step = Step::Ready;
        break;

    case Step::ConnectWindow:
        RunConnectWindow();
        break;

    case Step::BigButtonPushed:
        WaitFor("_cmn_msg_in", "cmn_msg_in", Step::QuestionOpening);
        break;

    case Step::QuestionOpening:
        DropMenuFocus();
        step = Step::Ready;
        break;

    case Step::AnswerShown:
        FinishAnswer();
        break;

    case Step::ClosePushed:
        SetVisible("T_Dialog", false);
        WaitFor("_hmMenu_fnsh", "hmMenu_fnsh", Step::Closing);
        break;

    case Step::Closing:
        Finish(HomeMenuOutcome::Closed);
        break;

    default:
        break;
    }
}

// The opening animation is over (state 1): the remote settings show the volume and the rumble, and the menu sounds.
void HomeButtonMenu::FinishOpening()
{
    ShowVolume();
    ShowRumbleSetting();
    Play(HomeMenuSound::HomeButton);
    step = Step::Ready;
}

// The remote settings have come up or gone down (state 10).
void HomeButtonMenu::FinishRemoteSettingsMove()
{
    lastTopBarAnimation.reset();
    lastBottomBarAnimation.reset();
    pendingTopBarAnimation.reset();
    pendingBottomBarAnimation.reset();

    if (part == Part::RemoteSettings)
    {
        Play(HomeMenuSound::OpenRemoteSettings);
        WaitFor("_cntrl_wndw_opn", "cntrl_wndw_opn", Step::WaitingForAnimation);
        return;
    }

    for (std::string_view paneName : HomeMenuNames::RemoteSettingsAreaPanes)
        SetVisible(std::string(paneName), false);
    step = Step::Ready;
}

// The end of a closing animation (state 8). The message of the reconnection and the question go away with theirs.
void HomeButtonMenu::FinishWaiting()
{
    if (waitedAnimation == &Animation("_link_msg_out", "link_msg_out"))
    {
        SetVisible("T_msg_00", false);
        SetVisible("T_msg_01", false);
    }

    if (waitedAnimation == &Animation("_cmn_msg_rtrn", "cmn_msg_rtrn"))
    {
        Start("_cmn_msg_btn_out", "msgBtn_00_inOut");
        Start("_cmn_msg_btn_out", "msgBtn_01_inOut");
        SetVisible("T_Dialog", false);
    }

    step = Step::Ready;
}

// Reconnect was pressed (state 3): when the button has been pushed the message comes, and after 30 steps every remote is disconnected.
void HomeButtonMenu::RunReconnectStart()
{
    HomeMenuAnimation& message = Animation("_link_msg_in", "link_msg_in");
    if (!waitedAnimation->IsPlaying() && waitedAnimation != &message)
    {
        HideBatteries();
        message.Start();
        waitedAnimation = &message;
    }

    --stepsUntilDisconnect;
    if (stepsUntilDisconnect > 0)
        return;

    for (int player = 0; player < PlayerCount; ++player)
        remotes.Disconnect(player);
    step = Step::WaitingForDisconnect;
}

// The window in which remotes connect again (states 5 and 6): 60 seconds, or 30 steps more once four remotes are connected.
void HomeButtonMenu::RunConnectWindow()
{
    if (stepsInConnectWindow == 0)
    {
        for (std::string_view button : HomeMenuNames::RemoteSettingsButtons)
            Start("_optn_btn_out", std::string(button) + "_inOut");

        DropMenuFocus();
        Animation("_12btn_on", "12btn_on").SetLooping(true);
        Start("_12btn_on", "12btn_on");
    }

    bool areFourConnected = input.ConnectedPlayers().all();
    if (areFourConnected && stepsInConnectWindow < ConnectWindowSteps - ConnectWindowStepsLeftWhenFourAreConnected)
        stepsInConnectWindow = ConnectWindowSteps - ConnectWindowStepsLeftWhenFourAreConnected;

    ++stepsInConnectWindow;
    if (stepsInConnectWindow > ConnectWindowSteps)
        step = Step::ConnectWindowClosing;
}

// The window ends (state 7): the message goes away and the buttons 1 and 2 of the picture stop blinking at the end of their round.
void HomeButtonMenu::FinishConnectWindow()
{
    LeaveAllAreas();
    Animation("_12btn_on", "12btn_on").SetLooping(false);
    Play(HomeMenuSound::ConnectWindowCloses);
    WaitFor("_link_msg_out", "link_msg_out", Step::WaitingForAnimation);
}

// The answer button has been pushed (state 13): No takes the question away, Yes fades the picture to black.
void HomeButtonMenu::FinishAnswer()
{
    LeaveAllAreas();
    if (hasChosenButton)
    {
        step = Step::FadingOut;
        fadeSteps = 0;
        return;
    }

    WaitFor("_cmn_msg_rtrn", "cmn_msg_rtrn", Step::WaitingForAnimation);
}

// The end of the menu (state 17): the rumble of every remote stops.
void HomeButtonMenu::Finish(HomeMenuOutcome howItEnded)
{
    for (int player = 0; player < PlayerCount; ++player)
        StopRumble(player);

    outcome = howItEnded;
    step = Step::Finished;
}

void HomeButtonMenu::WaitFor(const std::string& suffix, const std::string& groupName, Step nextStep)
{
    HomeMenuAnimation& animation = Animation(suffix, groupName);
    animation.Start();
    waitedAnimation = &animation;
    step = nextStep;
}

// The ten bars of the volume (0x813729C8): the bars up to the volume are yellow, the others gray.
void HomeButtonMenu::ShowVolume()
{
    for (int bar = 0; bar < HighestSpeakerVolumeStep; ++bar)
    {
        bool isLit = bar < speakerVolumeStep;
        Start(isLit ? "_sound_ylw" : "_sound_gry", HomeMenuNames::VolumeBarGroup(bar));
    }
}

// The rumble buttons (0x81372B24): the chosen one is white, the other one yellow.
void HomeButtonMenu::ShowRumbleSetting()
{
    if (isRumbleOn)
    {
        Start("_vb_btn_wht_psh", "optnBtn_10_psh");
        Start("_vb_btn_ylw_psh", "optnBtn_11_psh");
        return;
    }

    Start("_vb_btn_ylw_psh", "optnBtn_10_psh");
    Start("_vb_btn_wht_psh", "optnBtn_11_psh");
}

void HomeButtonMenu::Play(HomeMenuSound sound)
{
    if (playSound)
        playSound(sound);
}

}
