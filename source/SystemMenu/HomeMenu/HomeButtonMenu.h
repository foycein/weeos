#pragma once

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/HomeMenu/HomeMenuAnimation.h"
#include "SystemMenu/HomeMenu/HomeMenuFiles.h"
#include "SystemMenu/InputManager.h"

#include "Ios/Remote/RemoteService.h"
#include "Layout/LiveLayout.h"
#include "Layout/PanePlacement.h"
#include "Layout/TextFonts.h"

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The sounds of the HOME menu are the first 22 sounds of the sound archive of the menu, with these numbers.
enum class HomeMenuSound : std::size_t
{
    HomeButton = 0,
    ReturnToProgram = 1,
    GoToWiiMenu = 2,
    ResetProgram = 3,
    Focus = 4,
    Select = 5,
    Cancel = 6,
    OpenRemoteSettings = 7,
    CloseRemoteSettings = 8,
    VolumeUp = 9,
    VolumeDown = 10,
    VolumeUpToTheTop = 11,
    VolumeDownToTheBottom = 12,
    NothingDone = 13,
    RumbleOn = 14,
    RumbleOff = 15,
    ConnectWindowOpens = 16,

    // The sound of player 1. Player 2 has the next number, and so on.
    Connected = 17,

    ConnectWindowCloses = 21,
};

// The name of the sound in the sound archive, for example "HOMESE_FOCUS".
std::string_view NameOfHomeMenuSound(HomeMenuSound sound);

using HomeMenuSoundPlayer = std::function<void(HomeMenuSound sound)>;

enum class HomeMenuOutcome
{
    Open,

    // The user closed the menu with Close or the HOME button: the program under it goes on.
    Closed,

    // The user chose the big button and said yes, and the picture faded to black: the program under it ends.
    // The big button of the System Menu is "Wii Menu", which starts the menu again.
    ButtonChosen,
};

// The speaker of a remote has 10 steps of volume.
constexpr int HighestSpeakerVolumeStep = 10;

// The HOME Button Menu: the bar at the top with Close, the big button "Wii Menu" with its question, and the bar at the bottom with the
// batteries of the four remotes, which opens the Wii Remote Settings (volume of the speaker, rumble, connection of the remotes).
// This is the HBM library inside the System Menu (0x81371B74 to 0x81378000, docs/audit/SystemMenu.md). The hands are drawn by the
// cursor manager: its layouts are the same files as the hands of the HOME menu.
class HomeButtonMenu
{
public:
    // Everything given must outlive the menu. The fonts and the sound player may be missing: then the texts are not drawn and nothing sounds.
    HomeButtonMenu(Display& display, const HomeMenuFiles& files, const InputManager& input, Ios::RemoteService& remotes,
        CursorManager& cursors, const Layout::TextFonts* textFonts, HomeMenuSoundPlayer playSound);

    // Opens the menu from its first frame. The volume of the speakers of the remotes is a step from 0 to 10.
    void Open(int speakerVolumeStep);

    // One step: moves the animations, runs the part of the menu that is on, and reads the remotes.
    void Update();

    // The layout, the hands and the black of the fade.
    void Draw();

    HomeMenuOutcome Outcome() const;

    // The big button that was chosen when the outcome is ButtonChosen: 0 is the first one, "Wii Menu". The HOME menu of a channel
    // has a second one, "Reset", and some a third one for the manual.
    std::size_t ChosenBigButton() const;

    // The volume the user has set. The System Menu keeps it as BT.SPKV.
    int SpeakerVolumeStep() const;

    // The master volume of the console, from 1 to 0: it falls with the picture when the menu fades out to end the program under it
    // (0x81377118, the state 0x13 of 0x81372E9C). It is 1 at every other time.
    float SoundVolume() const;

private:
    // The steps of the menu (the states of the original, 0x81372E9C).
    enum class Step
    {
        Opening,
        Ready,
        RemoteSettingsMoving,
        WaitingForAnimation,
        ShowingRumble,
        ReconnectStarting,
        WaitingForDisconnect,
        ConnectWindow,
        ConnectWindowClosing,
        BigButtonPushed,
        QuestionOpening,
        AnswerShown,
        ClosePushed,
        Closing,
        FadingOut,
        Finished,
    };

    // Which part of the menu the pointer works in.
    enum class Part
    {
        Menu,
        RemoteSettings,
        Question,
    };

    // The places the pointer can point at.
    enum class Area
    {
        BigButton,
        Close,
        RemoteSettingsBar,
        VolumeDown,
        VolumeUp,
        RumbleOn,
        RumbleOff,
        Reconnect,
        Yes,
        No,
        SecondBigButton,
        ThirdBigButton,
    };

    // The big buttons in their order, and the groups of their panes in the layout.
    static constexpr std::array<Area, 3> BigButtonAreas = {Area::BigButton, Area::SecondBigButton, Area::ThirdBigButton};
    static constexpr std::array<std::string_view, 3> BigButtonGroups = {"btnL_00", "btnL_01", "btnL_10"};

    // An animation of the layout: the end of its file name and the group it plays on.
    struct AnimationName
    {
        std::string suffix;
        std::string group;

        bool operator==(const AnimationName&) const = default;
    };

    struct PlayerRecord
    {
        bool isConnected = false;
        std::optional<Area> pointedArea;
        bool isRumbling = false;
        int focusRumbleStepsLeft = 0;
        int rumblePauseStepsLeft = 0;
        int connectRumbleStepsLeft = 0;
        int connectSoundStepsLeft = 0;
    };

    // The times of the menu, in steps of 1001/60000 s.
    static constexpr int BatteryCheckIntervalSteps = 102;
    static constexpr int StepsBetweenFocusSounds = 3;
    static constexpr int StepsBeforeTheRemotesAreDisconnected = 30;

    // The connect window stays open for 60 seconds.
    static constexpr int ConnectWindowSteps = 3600;

    // A remote rumbles for 3 steps when its pointer reaches a button, and then not again for 9 steps.
    static constexpr int FocusRumbleSteps = 3;
    static constexpr int PauseAfterFocusRumbleSteps = 9;

    // Animations and panes (HomeButtonMenu.cpp)
    void BindAllAnimations();
    void HideBigButtonsNotOffered();
    void BindAnimation(const std::string& suffix, const std::string& groupName);
    HomeMenuAnimation& Animation(const AnimationName& name);
    HomeMenuAnimation& Animation(const std::string& suffix, const std::string& groupName);
    void Start(const std::string& suffix, const std::string& groupName);
    void Stop(const std::string& suffix, const std::string& groupName);
    bool IsPlaying(const std::string& suffix, const std::string& groupName);
    void StepAnimations();
    Formats::Pane& PaneNamed(const std::string& paneName);
    void SetVisible(const std::string& paneName, bool isVisible);
    void SetText(const std::string& paneName, const std::u16string& text);
    void FitToScreenShape();

    // The steps (HomeButtonMenu.cpp)
    void RunStep();
    void RunStepAfterAnimation();
    void FinishOpening();
    void FinishRemoteSettingsMove();
    void FinishWaiting();
    void RunReconnectStart();
    void RunConnectWindow();
    void FinishConnectWindow();
    void FinishAnswer();
    void Finish(HomeMenuOutcome howItEnded);
    void WaitFor(const std::string& suffix, const std::string& groupName, Step nextStep);
    void ShowVolume();
    void ShowRumbleSetting();
    void Play(HomeMenuSound sound);

    // The remotes (HomeMenuRemotes.cpp)
    void UpdateRemotes();
    void UpdatePlayer(int player);
    void NoticeConnect(int player);
    void NoticeDisconnect(int player);
    void ShowBattery(int player, int batteryLevel);
    void HideBatteries();
    void UpdateRumble(int player);
    void StartRumble(int player);
    void StopRumble(int player);
    void PlaySpeakerSound(int player, const Input::SpeakerSound& sound);

    // The pointer and the buttons (HomeMenuControls.cpp)
    void HandleButtonsOfPlayer(int player, const Input::RemoteState& state, Input::DeviceKind kind);
    std::optional<Area> AreaAt(const Input::RemoteState& state) const;
    bool IsActive(Area area) const;
    void MovePointer(int player, std::optional<Area> newArea);
    void EnterArea(int player, Area area);
    bool StartFocus(Area area);
    void LeaveArea(Area area);
    void LeaveAllAreas();
    void DropMenuFocus();
    void Press(Area area);
    std::optional<std::size_t> OfferedBigButtonOf(Area area) const;
    void PressBigButton(std::size_t bigButton);
    void PressClose();
    void OpenRemoteSettings();
    void CloseRemoteSettings();
    void PressVolumeDown();
    void PressVolumeUp();
    void ChangeVolume(int newStep, const std::string& pushedGroup, HomeMenuSound sound, HomeMenuSound soundAtTheEnd);
    void PressRumbleOn();
    void PressRumbleOff();
    void PressReconnect();
    void PressAnswer(bool isYes);
    AnimationName BottomBarFocusAnimation(bool isEntering) const;
    void RequestTopBarAnimation(const AnimationName& name);
    void RequestBottomBarAnimation(const AnimationName& name);
    void StartPendingBarAnimations();
    bool AreTopBarAnimationsIdle();
    bool AreBottomBarAnimationsIdle();
    void PlayFocusSound();
    static std::string RemoteSettingsButtonOf(Area area);

    Display& display;
    const HomeMenuFiles& files;
    const InputManager& input;
    Ios::RemoteService& remotes;
    CursorManager& cursors;
    const Layout::TextFonts* fonts;
    HomeMenuSoundPlayer playSound;

    Layout::LiveLayout layout;
    Layout::PlacementSettings placementSettings;
    std::map<std::string, Formats::LayoutAnimation> animationFiles;
    std::vector<std::unique_ptr<HomeMenuAnimation>> animations;
    std::map<std::string, HomeMenuAnimation*> animationsByName;

    Step step = Step::Finished;
    Part part = Part::Menu;
    HomeMenuOutcome outcome = HomeMenuOutcome::Closed;
    HomeMenuAnimation* waitedAnimation = nullptr;
    bool hasChosenButton = false;
    std::size_t chosenBigButton = 0;

    int speakerVolumeStep = 0;
    bool isRumbleOn = true;
    std::array<PlayerRecord, PlayerCount> players;
    std::map<Area, int> pointersOnArea;

    // A bar that is still moving gets its next focus animation when it has stopped, unless that one was the last one it played.
    std::optional<AnimationName> pendingTopBarAnimation;
    std::optional<AnimationName> pendingBottomBarAnimation;
    std::optional<AnimationName> lastTopBarAnimation;
    std::optional<AnimationName> lastBottomBarAnimation;

    int stepsSinceFocusSound = StepsBetweenFocusSounds;
    int stepsUntilBatteryCheck = 0;
    int stepsUntilDisconnect = 0;
    int stepsInConnectWindow = 0;
    int fadeSteps = 0;
};

}
