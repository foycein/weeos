#include "SystemMenu/HomeMenu/HomeButtonMenu.h"

#include "SystemMenu/HomeMenu/HomeMenuNames.h"

// The four remotes as the HOME menu shows them: the battery of each player, its rumble, and the sounds of its speaker
// (0x813742E0, 0x81373DD0, 0x81376750 and the rumble timers at the start of 0x81372E9C).

namespace SystemMenu
{

namespace
{

constexpr int BatteryBarCount = 4;

// Below this level the battery shows red.
constexpr int LowBatteryLevel = 2;

// A remote that connects rumbles for 300 milliseconds, and its speaker sounds 400 milliseconds after the connection, in steps of
// 1001/60000 s.
constexpr int ConnectRumbleSteps = 18;
constexpr int ConnectSoundDelaySteps = 24;

}

void HomeButtonMenu::UpdateRemotes()
{
    for (int player = 0; player < PlayerCount; ++player)
        UpdatePlayer(player);

    --stepsUntilBatteryCheck;
    if (stepsUntilBatteryCheck > 0)
        return;

    stepsUntilBatteryCheck = BatteryCheckIntervalSteps;
    for (int player = 0; player < PlayerCount; ++player)
    {
        if (input.Player(player))
            ShowBattery(player, input.Player(player)->state.batteryLevel);
    }
}

void HomeButtonMenu::UpdatePlayer(int player)
{
    const std::optional<PlayerInput>& playerInput = input.Player(player);
    PlayerRecord& record = players[player];

    if (!playerInput)
    {
        if (record.isConnected)
            NoticeDisconnect(player);

        MovePointer(player, std::nullopt);
        return;
    }

    if (!record.isConnected)
        NoticeConnect(player);

    if (record.connectSoundStepsLeft > 0)
    {
        --record.connectSoundStepsLeft;
        if (record.connectSoundStepsLeft == 0)
            PlaySpeakerSound(player, files.connectSounds[player]);
    }

    if (step == Step::Ready)
    {
        HandleButtonsOfPlayer(player, playerInput->state, playerInput->deviceKind);
        return;
    }

    // In the connect window any button of a remote that is connected ends the window.
    bool hasMessageArrived = !IsPlaying("_link_msg_in", "link_msg_in");
    bool hasPressedAButton = playerInput->state.buttonsPressed != 0 || playerInput->state.classic.buttonsPressed != 0;
    if (step == Step::ConnectWindow && hasMessageArrived && hasPressedAButton)
        stepsInConnectWindow = ConnectWindowSteps;
}

// The battery blinks white, the remote rumbles, and a sound of its own plays on the console and then in its speaker.
void HomeButtonMenu::NoticeConnect(int player)
{
    PlayerRecord& record = players[player];
    record.isConnected = true;

    std::string group = HomeMenuNames::PlayerGroup(player);
    Start("_btry_wht", group);
    Stop("_btry_wink_gry", group);
    Start("_btry_wink", group);

    std::size_t soundOfThePlayer = static_cast<std::size_t>(HomeMenuSound::Connected) + static_cast<std::size_t>(player);
    Play(static_cast<HomeMenuSound>(soundOfThePlayer));
    record.connectSoundStepsLeft = ConnectSoundDelaySteps;

    StartRumble(player);
    record.connectRumbleStepsLeft = ConnectRumbleSteps;
    ShowBattery(player, input.Player(player)->state.batteryLevel);
}

void HomeButtonMenu::NoticeDisconnect(int player)
{
    std::string group = HomeMenuNames::PlayerGroup(player);
    Start("_btry_wht", group);
    Start("_btry_wink_gry", group);

    for (int bar = 0; bar < BatteryBarCount; ++bar)
        SetVisible(HomeMenuNames::BatteryBarPane(player, bar), false);

    StopRumble(player);
    std::optional<Area> pointedArea = players[player].pointedArea;
    players[player] = PlayerRecord();
    players[player].pointedArea = pointedArea;
}

// One bar for each level of the battery, white, or red when the battery is low.
void HomeButtonMenu::ShowBattery(int player, int batteryLevel)
{
    for (int bar = 0; bar < BatteryBarCount; ++bar)
        SetVisible(HomeMenuNames::BatteryBarPane(player, bar), bar < batteryLevel);

    Start(batteryLevel < LowBatteryLevel ? "_btry_red" : "_btry_wht", HomeMenuNames::PlayerGroup(player));
}

void HomeButtonMenu::HideBatteries()
{
    for (int player = 0; player < PlayerCount; ++player)
    {
        for (int bar = 0; bar < BatteryBarCount; ++bar)
            SetVisible(HomeMenuNames::BatteryBarPane(player, bar), false);
    }
}

void HomeButtonMenu::UpdateRumble(int player)
{
    PlayerRecord& record = players[player];

    if (record.connectRumbleStepsLeft > 0)
    {
        --record.connectRumbleStepsLeft;
        if (record.connectRumbleStepsLeft == 0)
            StopRumble(player);
    }

    if (record.focusRumbleStepsLeft <= 0)
    {
        if (record.rumblePauseStepsLeft > 0)
            --record.rumblePauseStepsLeft;
        return;
    }

    if (!record.isConnected || !record.isRumbling)
    {
        record.focusRumbleStepsLeft = 0;
        record.rumblePauseStepsLeft = 0;
        StopRumble(player);
        return;
    }

    --record.focusRumbleStepsLeft;
    if (record.focusRumbleStepsLeft > 0)
        return;

    StopRumble(player);
    record.rumblePauseStepsLeft = PauseAfterFocusRumbleSteps;
}

// The remote service drops the start while the vibration setting is off, as the library of the original does.
void HomeButtonMenu::StartRumble(int player)
{
    players[player].isRumbling = true;
    remotes.SetRumble(player, true);
}

void HomeButtonMenu::StopRumble(int player)
{
    if (!players[player].isRumbling)
        return;

    players[player].isRumbling = false;
    remotes.SetRumble(player, false);
}

// The menu turns the amplifier of the speaker up while it is open and scales the samples with the volume instead (INP-01).
void HomeButtonMenu::PlaySpeakerSound(int player, const Input::SpeakerSound& sound)
{
    Input::SpeakerSound scaledSound = sound;
    for (std::int16_t& sample : scaledSound.samples)
        sample = static_cast<std::int16_t>(sample * speakerVolumeStep / HighestSpeakerVolumeStep);

    remotes.PlaySpeakerSound(player, scaledSound);
}

}
