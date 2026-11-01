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

// The code that the Wii Speak Channel shows when the login server of Nintendo Wi-Fi Connection, whose place the owner's server takes,
// cannot be reached while the Internet works. The codes of the SDK's Wi-Fi Connection library were not read (CHN-04, Assumed).
constexpr std::uint32_t LoginServerUnreachableCode = 20100;

// The buttons of 02000_button and the panes of the ones WeeOS does not have.
const std::string WiiMenuButton = "B_dbutton_00";
const std::string ChooseMiisButton = "B_dbutton_01";
const std::string ShowPhotosButton = "B_dbutton_02";
const std::string HelpButton = "B_dbutton_03";
const std::string SettingsButton = "B_dbutton_04";
const std::string WiiNumberButton = "B_dbutton_05";
const std::string CreateMessageButton = "B_dbutton_06";
const std::string AddressBookButton = "B_dbutton_07";
const std::string LobbyButton = "B_dbutton_08";

struct ButtonLabel
{
    std::string_view pane;
    std::uint32_t messageId;
};

constexpr std::array<ButtonLabel, 9> ButtonLabels = {{
    {"TextBox_00", WiiSpeakText::ChatRoomButtonWiiMenu},
    {"TextBox_01", WiiSpeakText::ChatRoomButtonChooseMiis},
    {"TextBox_02", WiiSpeakText::ChatRoomButtonShowPhotos},
    {"TextBox_05", WiiSpeakText::ChatRoomButtonWiiNumber},
    {"TextBox_07", WiiSpeakText::ChatRoomButtonAddressBook},
    {"TextBox_03", WiiSpeakText::ChatRoomButtonHelp},
    {"TextBox_04", WiiSpeakText::ChatRoomButtonSettings},
    {"TextBox_06", WiiSpeakText::ChatRoomButtonCreateMessage},
    {"TextBox_08", WiiSpeakText::ChatRoomButtonLobby},
}};

const std::string BackgroundWaves = "02000_bg_wait_00";
const std::string SharedPhotoName = "SharedPhoto";

// The last frame of the photo coming out of the slot.
constexpr float PrintedPhotoFrame = 65.0f;
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
constexpr std::string_view ChooseMiisSound = "SE_RIP_CLK_BTN_SELECT_MII";
constexpr std::string_view ShowPhotosSound = "SE_RIP_CLK_BTN_PHOTO";
constexpr std::string_view WiiNumberSound = "SE_RIP_CLK_BTN_MY_NUMBER";
constexpr std::string_view AddressBookSound = "SE_RIP_CLK_BTN_ADDRESS";
constexpr std::string_view PhotoAppearSound = "SE_RIP_PHOTO_APPEAR";
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
    for (const std::string& button : {WiiMenuButton, ChooseMiisButton, ShowPhotosButton, HelpButton, SettingsButton, WiiNumberButton,
             CreateMessageButton, AddressBookButton, LobbyButton})
        buttons.AddButton(button);

    std::size_t wavesSlot = background.Layout().BindAnimationToPane(BackgroundWaves, "RootPane", Layout::PlaybackMode::Loop);
    background.Layout().StartAnimation(wavesSlot);
    buttons.PlayAnimation("allbutton_00_in");
    ShowTexts();
    ShowMute();
    ShowChosenMiis();
    ShowSharedPhoto();
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
    if (miisInCircle)
        miisInCircle->Calculate();
    if (photo)
        photo->Calculate();
    return nextScreen;
}

void ChatRoomScreen::Draw()
{
    background.Draw();
    circle.Draw();
    if (miisInCircle)
        miisInCircle->Draw();
    if (photo)
        photo->Draw();
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

    if (button == ChooseMiisButton && session.consoleMiis.empty())
    {
        session.services.sounds.Request(ChooseMiisSound);
        session.ShowMessage(WiiSpeakText::NoMiisToChoose, {WiiSpeakText::Ok});
        return std::nullopt;
    }

    if (button == ChooseMiisButton)
    {
        session.services.sounds.Request(ChooseMiisSound);
        return WiiSpeakScreenName::ChooseMiis;
    }

    if (button == ShowPhotosButton)
    {
        session.services.sounds.Request(ShowPhotosSound);
        return WiiSpeakScreenName::ShowPhotos;
    }

    if (button == WiiNumberButton)
    {
        session.services.sounds.Request(WiiNumberSound);
        ShowWiiNumber();
        return std::nullopt;
    }

    if (button == AddressBookButton)
    {
        session.services.sounds.Request(AddressBookSound);
        return WiiSpeakScreenName::AddressBook;
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

// The lobby logs in to Nintendo Wi-Fi Connection first, whose place the owner's server takes (CHN-04). While the PC is not connected the
// network does not start; else the try reaches for the server beside the program.
void ChatRoomScreen::StartConnecting()
{
    ChannelNetwork& network = session.services.network;
    server.reset();
    networkStart = SystemMenu::StartNetwork(network.configuration, network.sockets);
    if (networkStart.isOnline)
        server = std::make_unique<SystemMenu::ServerContact>(network.sockets, network.serverAddress);

    connectionStartSeconds = session.services.clock.Seconds();
    session.ShowMessage(WiiSpeakText::ConnectingToWiFiConnection, {});
    waiting = Waiting::Connecting;
}

// The code of the failed login: the support code of SOStartup after its time-out, or the code of a login server that cannot be reached.
std::optional<std::uint32_t> ChatRoomScreen::FindConnectionFailure()
{
    if (!networkStart.isOnline)
    {
        double waitedSeconds = session.services.clock.Seconds() - connectionStartSeconds;
        if (waitedSeconds < networkStart.secondsUntilFailure)
            return std::nullopt;

        return networkStart.supportCode;
    }

    if (!server->Answer())
        return std::nullopt;

    return LoginServerUnreachableCode;
}

void ChatRoomScreen::FollowTheMessageWindow()
{
    if (waiting == Waiting::Connecting)
    {
        std::optional<std::uint32_t> failureCode = FindConnectionFailure();
        if (failureCode)
            session.dialog.Close();

        if (session.dialog.IsOpen() || !failureCode)
            return;

        std::u16string code = AsciiToText(std::to_string(*failureCode));
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

// The chosen Miis stand in the circle: the layout 03000_mii_0N for N Miis, each Mii on its pane P_mii_01 to P_mii_0N.
void ChatRoomScreen::ShowChosenMiis()
{
    std::vector<Mii::MiiRecord> chosen = session.saveData.ChosenMiis();
    if (chosen.empty())
        return;

    std::string layoutName = std::format("03000_mii_{:02}", chosen.size());
    miisInCircle = std::make_unique<SceneLayout>(session.services.display, session.files.SystemLayouts(layoutName + ".arc"), layoutName);
    miisInCircle->SetText("T_name_00", u"");
    for (std::size_t place = 0; place < chosen.size(); ++place)
    {
        const Mii::MiiRecord& mii = chosen[place];
        std::string pane = std::format("P_mii_{:02}", place + 1);
        miisInCircle->ShowMadeTexture(pane, MiiFaces::TextureNameOf(mii), session.miiFaces.FaceOf(mii));
    }
}

// The photo that Show Photos chose comes out of the slot at the bottom (12410_photoap of the scene photoAppre).
void ChatRoomScreen::ShowSharedPhoto()
{
    if (!session.sharedPhoto)
        return;

    photo = std::make_unique<SceneLayout>(session.services.display, session.files.SceneLayouts("photoAppre", "12410_photoap.arc"),
        "12410_photoap");
    photo->ShowMadeTexture("P_photoA_00", SharedPhotoName, *session.sharedPhoto);
    if (!session.isSharedPhotoNew)
    {
        photo->ShowAnimationFrame("all_in", PrintedPhotoFrame);
        return;
    }

    photo->PlayAnimation("all_in");
    session.services.sounds.Request(PhotoAppearSound);
    session.isSharedPhotoNew = false;
}

// The Wii number of this console from the friend server, or the WiiConnect24 window while there is no server.
void ChatRoomScreen::ShowWiiNumber()
{
    std::optional<std::uint64_t> wiiNumber = session.friends.OwnWiiNumber();
    if (!wiiNumber)
    {
        session.ShowWiiConnect24Unreachable();
        return;
    }

    session.dialog.Show(session.Text(WiiSpeakText::YourWiiNumber, WiiNumberText(*wiiNumber)), {session.Text(WiiSpeakText::Ok)});
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
