#include "Channels/WiiSpeak/ChatRoomScreen.h"

#include "Channels/ChannelText.h"
#include "Channels/WiiSpeak/MicrophoneLevel.h"
#include "Channels/WiiSpeak/WiiSpeakMessageIds.h"

#include <array>
#include <ctime>
#include <format>
#include <string>

namespace Channels
{

namespace
{

constexpr std::string_view SceneName = "talk";

// The buttons of 02000_button and the panes of the ones WeeOS does not have.
const std::string WiiMenuButton = "B_dbutton_00";
const std::string HelpButton = "B_dbutton_03";
const std::string SettingsButton = "B_dbutton_04";
const std::string CreateMessageButton = "B_dbutton_06";
const std::string LobbyButton = "B_dbutton_08";
constexpr std::array<std::string_view, 4> ButtonsNotBuilt = {"N_button_01", "N_button_02", "N_button_05", "N_button_07"};

struct ButtonLabel
{
    std::string_view pane;
    std::uint32_t messageId;
};

constexpr std::array<ButtonLabel, 5> ButtonLabels = {{
    {"TextBox_00", WiiSpeakText::ChatRoomButtonWiiMenu},
    {"TextBox_03", WiiSpeakText::ChatRoomButtonHelp},
    {"TextBox_04", WiiSpeakText::ChatRoomButtonSettings},
    {"TextBox_06", WiiSpeakText::ChatRoomButtonCreateMessage},
    {"TextBox_08", WiiSpeakText::ChatRoomButtonLobby},
}};

const std::string BackgroundWaves = "02000_bg_wait_00";
const std::string CircleLightOn = "on_00";
const std::string CircleLightOff = "off_00";
const std::string MuteOn = "micOff_00";
const std::string MuteOff = "micOn_00";

// The sounds of the chat room, by their names in MicChatSound.brsar.
constexpr std::string_view WiiMenuSound = "SE_RIP_CLK_BTN_WII_MENU";
constexpr std::string_view HelpSound = "SE_RIP_CLK_BTN_HELP";
constexpr std::string_view SettingsSound = "SE_RIP_CLK_BTN_CONFIG";
constexpr std::string_view CreateMessageSound = "SE_RIP_CLK_BTN_MK_LETTER";
constexpr std::string_view LobbySound = "SE_RIP_CLK_BTN_LOBBY";
constexpr std::string_view MuteOnSound = "SE_COM_MUTE_ON";
constexpr std::string_view MuteOffSound = "SE_COM_MUTE_OFF";

constexpr int HoursOfHalfADay = 12;
constexpr int FirstYearOfTheCalendar = 1900;
constexpr int DaysOfTheWeek = 7;
constexpr int DigitBase = 10;

constexpr SystemMenu::ButtonSet MuteButtons = {Input::RemoteButtons::Minus, Input::ClassicButtons::Minus};

std::u16string DigitText(int digit)
{
    return AsciiToText(std::to_string(digit));
}

}

ChatRoomScreen::ChatRoomScreen(WiiSpeakSession& chatSession)
    : session(chatSession)
    , background(session.services.display, session.files.SceneLayouts(SceneName, "02000_bg.arc"), "02000_bg")
    , buttons(session.services.display, session.files.SceneLayouts(SceneName, "02000_button.arc"), "02000_button")
    , clock(session.services.display, session.files.SceneLayouts(SceneName, "02000_clock.arc"), "02000_clock")
    , muteSign(session.services.display, session.files.SystemLayouts("02100_mute.arc"), "02100_mute")
    , circle(session.services.display, session.files.SystemLayouts("03000_circle.arc"), "03000_circle")
{
    for (const std::string& button : {WiiMenuButton, HelpButton, SettingsButton, CreateMessageButton, LobbyButton})
        buttons.AddButton(button);

    for (std::string_view pane : ButtonsNotBuilt)
        buttons.SetVisible(pane, false);

    std::size_t wavesSlot = background.Layout().BindAnimationToPane(BackgroundWaves, "RootPane", Layout::PlaybackMode::Loop);
    background.Layout().StartAnimation(wavesSlot);
    buttons.PlayAnimation("allbutton_00_in");
    ShowTexts();
    ShowMute();
}

std::optional<WiiSpeakScreenName> ChatRoomScreen::Update()
{
    bool hearsVoice = IsVoiceHeard(HeardLevelInDecibels(session.heardThisStep));
    if (hearsVoice != isCircleLit)
        circle.PlayAnimation(hearsVoice ? CircleLightOn : CircleLightOff);
    isCircleLit = hearsVoice;

    std::optional<WiiSpeakScreenName> nextScreen;
    if (session.dialog.IsOpen() || waiting != Waiting::Nothing)
    {
        FollowTheMessageWindow();
    }
    else
    {
        if (session.services.input.AnyPlayerPressed(MuteButtons))
            ToggleMute();

        std::optional<std::string> pressed = buttons.TakePressedButton(session.services.input);
        if (pressed)
            nextScreen = Press(*pressed);
    }

    ShowClock();
    for (SceneLayout* layout : {&background, &buttons, &clock, &muteSign, &circle})
        layout->Calculate();
    return nextScreen;
}

void ChatRoomScreen::Draw()
{
    background.Draw();
    circle.Draw();
    clock.Draw();
    muteSign.Draw();
    buttons.Draw();
}

std::vector<SceneLayout*> ChatRoomScreen::ShownLayouts()
{
    return {&background, &circle, &clock, &muteSign, &buttons};
}

bool ChatRoomScreen::IsCircleLit() const
{
    return isCircleLit;
}

void ChatRoomScreen::ShowTexts()
{
    for (const ButtonLabel& label : ButtonLabels)
        buttons.SetText(label.pane, session.Text(label.messageId));

    clock.SetText("T_mute", session.Text(WiiSpeakText::Mute));
    clock.SetText("T_volume", session.Text(WiiSpeakText::Volume));
    clock.SetText("T_change", session.Text(WiiSpeakText::ChangeVoice));
}

// The clock of the console: "January 5 (Mon)" over "PM 1:05", as the template 512258 of the messages orders the date.
void ChatRoomScreen::ShowClock()
{
    std::time_t now = std::time(nullptr);
    std::tm calendar = {};
    localtime_s(&calendar, &now);

    int weekdayFromMonday = (calendar.tm_wday + DaysOfTheWeek - 1) % DaysOfTheWeek;
    std::u16string date = session.Text(WiiSpeakText::January + calendar.tm_mon);
    date += u" " + AsciiToText(std::to_string(calendar.tm_mday));
    date += u" (" + session.Text(WiiSpeakText::Monday + weekdayFromMonday) + u")";
    clock.SetText("T_date_00", date);

    bool isAfternoon = calendar.tm_hour >= HoursOfHalfADay;
    int hour = calendar.tm_hour % HoursOfHalfADay;
    if (hour == 0)
        hour = HoursOfHalfADay;

    clock.SetText("T_time_00", session.Text(isAfternoon ? WiiSpeakText::Afternoon : WiiSpeakText::Morning));
    clock.SetText("T_time_01", hour >= DigitBase ? DigitText(hour / DigitBase) : u"");
    clock.SetText("T_time_02", DigitText(hour % DigitBase));
    clock.SetText("T_time_04", DigitText(calendar.tm_min / DigitBase));
    clock.SetText("T_time_05", DigitText(calendar.tm_min % DigitBase));
}

void ChatRoomScreen::ShowMute()
{
    muteSign.PlayAnimation(session.services.audio.microphone.IsMuted() ? MuteOn : MuteOff);
}

std::optional<WiiSpeakScreenName> ChatRoomScreen::Press(const std::string& button)
{
    if (button == WiiMenuButton)
    {
        session.services.sounds.Request(WiiMenuSound);
        session.ShowMessage(WiiSpeakText::ReturningToWiiMenu, {WiiSpeakText::Yes, WiiSpeakText::No});
        waiting = Waiting::AnswerToReturning;
        return std::nullopt;
    }

    if (button == HelpButton)
    {
        session.services.sounds.Request(HelpSound);
        session.ShowMessage(WiiSpeakText::ChatRoomGuide, {WiiSpeakText::Ok});
        return std::nullopt;
    }

    if (button == SettingsButton)
    {
        session.services.sounds.Request(SettingsSound);
        return WiiSpeakScreenName::SettingsMenu;
    }

    if (button == CreateMessageButton)
    {
        session.services.sounds.Request(CreateMessageSound);
        return WiiSpeakScreenName::CreateMessage;
    }

    session.services.sounds.Request(LobbySound);
    StartConnecting();
    return std::nullopt;
}

// The lobby logs in to Nintendo Wi-Fi Connection first. The connection fails as every connection of WeeOS does (CHN-04).
void ChatRoomScreen::StartConnecting()
{
    connection = TryToConnect(session.services.network);
    connectionStartSeconds = session.services.clock.Seconds();
    session.ShowMessage(WiiSpeakText::ConnectingToWiFiConnection, {});
    waiting = Waiting::Connecting;
}

void ChatRoomScreen::FollowTheMessageWindow()
{
    if (waiting == Waiting::Connecting)
    {
        double waitedSeconds = session.services.clock.Seconds() - connectionStartSeconds;
        if (waitedSeconds >= connection->secondsUntilFailure)
            session.dialog.Close();

        if (session.dialog.IsOpen())
            return;

        std::u16string code = AsciiToText(std::to_string(connection->supportCode));
        session.dialog.Show(session.Text(WiiSpeakText::UnableToConnectToTheInternet, code), {session.Text(WiiSpeakText::Ok)});
        waiting = Waiting::ConnectionError;
        return;
    }

    std::optional<int> choice = session.dialog.TakeChoice();
    if (!choice)
        return;

    if (waiting == Waiting::AnswerToReturning && *choice == 0)
        session.services.wiiMenu.Request();

    waiting = Waiting::Nothing;
}

void ChatRoomScreen::ToggleMute()
{
    Ios::MicrophoneService& microphone = session.services.audio.microphone;
    bool isMuted = !microphone.IsMuted();
    microphone.SetMuted(isMuted);
    session.services.sounds.Request(isMuted ? MuteOnSound : MuteOffSound);
    ShowMute();
}

}
