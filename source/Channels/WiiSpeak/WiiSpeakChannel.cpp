#include "Channels/WiiSpeak/WiiSpeakChannel.h"

#include "Channels/ChannelFiles.h"
#include "Channels/NativeChannels.h"
#include "Channels/WiiSpeak/ChatRoomScreen.h"
#include "Channels/WiiSpeak/CreateMessageScreen.h"
#include "Channels/WiiSpeak/PerformanceCheckScreen.h"
#include "Channels/WiiSpeak/SettingsScreens.h"
#include "Channels/WiiSpeak/WiiSpeakMessageIds.h"

#include "Formats/Font.h"
#include "Formats/FormatError.h"
#include "Layout/PaneRectangle.h"
#include "Ios/FileSystem/OpenFile.h"
#include "Ios/Title/TitlePaths.h"
#include "Mii/MiiDatabase.h"

#include <array>
#include <vector>

namespace Channels
{

namespace
{

constexpr std::string_view LayoutFontName = "pac_nRodDb_32_I4.brfnt";

// The hands are the ones of the HOME Button Menu in content 3: the channel's own hand (P1_Def.arc of system.szs) has no hands for
// players 2 to 4 (as for Forecast and News, CHN-26).
constexpr std::uint16_t HomeMenuContentIndex = 3;
constexpr std::string_view HandArchivePath = "./HomeButton3/LZ77_homeBtn.arc";

constexpr std::array<std::uint32_t, 3> WelcomeTexts = {WiiSpeakText::Welcome, WiiSpeakText::AboutChatting, WiiSpeakText::AboutVoiceMessages};

std::unique_ptr<Layout::LayoutArchive> ReadHandArchive(SystemMenu::TitleContents& contents)
{
    Formats::U8Archive homeMenuFiles = ReadArchiveContent(contents, HomeMenuContentIndex);
    return std::make_unique<Layout::LayoutArchive>(Formats::ReadU8Archive(ReadPackedFile(homeMenuFiles, HandArchivePath)));
}

}

WiiSpeakChannel::WiiSpeakChannel(ChannelServices& channelServices)
    : services(channelServices)
    , files(channelServices.contents)
    , messages(files.EnglishMessages())
    , hands(std::make_unique<SystemMenu::CursorManager>(channelServices.display, ReadHandArchive(channelServices.contents)))
    , dialog(channelServices.display, files)
    , saveFile(channelServices.storage.fileSystem, channelServices.storage.caller, Ios::DataFolderPath(WiiSpeakChannelTitleId))
    , session(channelServices, files, messages, dialog, saveFile)
{
    fonts.AddFont(services.display.Device(), Formats::ReadFont(files.LayoutFont()), {LayoutFontName});
    services.display.SetTextFonts(fonts.Fonts());
}

WiiSpeakChannel::~WiiSpeakChannel()
{
    screen.reset();
    services.audio.waves.StopAll();
    services.audio.microphone.CloseSession();
}

void WiiSpeakChannel::Update()
{
    HearTheMicrophone();
    if (stage == Stage::Starting)
        Start();
    else if (stage == Stage::Running)
    {
        std::optional<WiiSpeakScreenName> nextScreen = screen->Update();
        if (nextScreen)
            ChangeScreen(*nextScreen);
    }
    else
        FollowTheFirstStart();

    dialog.Update(services.input, services.sounds);
}

void WiiSpeakChannel::Draw()
{
    if (screen)
        screen->Draw();

    dialog.Draw();
}

SystemMenu::CursorManager& WiiSpeakChannel::Hands()
{
    return *hands;
}

WiiSpeakChannel::Stage WiiSpeakChannel::CurrentStage() const
{
    return stage;
}

std::optional<WiiSpeakScreenName> WiiSpeakChannel::CurrentScreenName() const
{
    return screenName;
}

WiiSpeakScreen* WiiSpeakChannel::CurrentScreen()
{
    return screen.get();
}

WiiSpeakSession& WiiSpeakChannel::Session()
{
    return session;
}

std::size_t WiiSpeakChannel::MiiCount() const
{
    return miiCount;
}

std::optional<Formats::Vector2> WiiSpeakChannel::PlaceOfPane(std::string_view paneName)
{
    std::vector<SceneLayout*> layouts;
    if (screen)
        layouts = screen->ShownLayouts();
    if (dialog.IsOpen())
        layouts.push_back(&dialog.Layout());

    for (auto layout = layouts.rbegin(); layout != layouts.rend(); ++layout)
    {
        const Formats::Pane* pane = (*layout)->Layout().FindPane(paneName);
        if (pane == nullptr)
            continue;

        Layout::PaneRectangle rectangle = Layout::RectangleOfPane(*pane);
        Formats::Vector2 middle = {rectangle.left + rectangle.width / 2, rectangle.top - rectangle.height / 2};
        std::optional<Formats::Vector2> place = (*layout)->Layout().PointOfPaneInPicture(paneName, middle);
        if (place)
            return place;
    }
    return std::nullopt;
}

// The checks of the first step, in the order of FirstSetUpScene: the Mii database, the accessory, the save file.
void WiiSpeakChannel::Start()
{
    if (!CheckMiiDatabase())
    {
        Refuse(WiiSpeakText::MiiChannelDataUnreadable);
        return;
    }

    if (!OpenMicrophone())
        return;

    ReadSaveFile();
}

// The face library opens the database at its start; a missing or broken file makes it unusable until the Mii Channel formats it.
bool WiiSpeakChannel::CheckMiiDatabase()
{
    Ios::NandPath path = Ios::NandPath::Parse(MiiDatabasePath).Value();
    Ios::Result<Ios::OpenFile> opened = services.storage.fileSystem.Open(path, Ios::OpenMode::Read, services.storage.caller);
    if (opened.Failed())
        return false;

    Ios::Result<Formats::Bytes> bytes = opened.Value().Read(Mii::MiiDatabaseFileSize);
    if (bytes.Failed())
        return false;

    try
    {
        Mii::MiiDatabase database = Mii::MiiDatabase::Read(bytes.Value());
        miiCount = 0;
        for (std::size_t slot = 0; slot < Mii::MiiSlotCount; ++slot)
        {
            if (database.FindMii(slot))
                ++miiCount;
        }
    }
    catch (const Formats::FormatError&)
    {
        return false;
    }
    return true;
}

bool WiiSpeakChannel::OpenMicrophone()
{
    Ios::MicrophoneOpenResult opened = services.audio.microphone.Open();
    if (opened == Ios::MicrophoneOpenResult::NotConnected)
    {
        Refuse(WiiSpeakText::AccessoryNotConnected);
        return false;
    }

    if (opened == Ios::MicrophoneOpenResult::DeviceProblem)
    {
        Refuse(WiiSpeakText::AccessoryProblem);
        return false;
    }
    return true;
}

// The channel cannot go on: its window says why, and its button returns to the Wii Menu.
void WiiSpeakChannel::Refuse(std::uint32_t messageId)
{
    session.ShowMessage(messageId, {WiiSpeakText::WiiMenu});
    stage = Stage::Refusing;
}

void WiiSpeakChannel::ReadSaveFile()
{
    LoadedSaveData loaded = saveFile.Load();
    session.saveData = loaded.data;
    services.audio.microphone.SetSensitivity(session.saveData.Sensitivity());

    if (loaded.state == SaveFileState::Read)
    {
        stage = Stage::Running;
        ChangeScreen(WiiSpeakScreenName::ChatRoom);
        return;
    }

    if (loaded.state == SaveFileState::Damaged)
    {
        session.ShowMessage(WiiSpeakText::SaveDataCorrupted, {WiiSpeakText::Yes, WiiSpeakText::No});
        stage = Stage::AskingForNewData;
        return;
    }

    stage = Stage::Welcoming;
    welcomeNumber = 0;
    ShowNextWelcome();
}

void WiiSpeakChannel::FollowTheFirstStart()
{
    if (stage == Stage::ShowingPrecautions)
    {
        if (screen->Update())
            FinishFirstStart();
        return;
    }

    std::optional<int> choice = dialog.TakeChoice();
    if (!choice)
        return;

    if (stage == Stage::Refusing)
        services.wiiMenu.Request();
    else if (stage == Stage::AskingForNewData && *choice == 0)
        FinishFirstStart();
    else if (stage == Stage::AskingForNewData)
        services.wiiMenu.Request();
    else if (stage == Stage::Welcoming)
        ShowNextWelcome();
    else if (stage == Stage::TellingSaveCreated)
    {
        stage = Stage::Running;
        ChangeScreen(WiiSpeakScreenName::ChatRoom);
    }
}

void WiiSpeakChannel::ShowNextWelcome()
{
    if (welcomeNumber < static_cast<int>(WelcomeTexts.size()))
    {
        session.ShowMessage(WelcomeTexts[welcomeNumber], {WiiSpeakText::Next});
        ++welcomeNumber;
        return;
    }

    stage = Stage::ShowingPrecautions;
    screen = std::make_unique<PrecautionsScreen>(session, WiiSpeakScreenName::ChatRoom);
    screenName = WiiSpeakScreenName::Precautions;
}

// The save file is made at the end of the first start, as the program does before "Save data created!".
void WiiSpeakChannel::FinishFirstStart()
{
    session.saveData = WiiSpeakSaveData();
    session.saveData.SetSensitivity(services.audio.microphone.Sensitivity());
    saveFile.Save(session.saveData);
    session.ShowMessage(WiiSpeakText::SaveDataCreated, {WiiSpeakText::Ok});
    stage = Stage::TellingSaveCreated;
}

void WiiSpeakChannel::HearTheMicrophone()
{
    Ios::MicrophoneService& microphone = services.audio.microphone;
    session.heardThisStep.resize(microphone.AvailableSampleCount());
    std::size_t taken = microphone.TakeSamples(session.heardThisStep);
    session.heardThisStep.resize(taken);
}

void WiiSpeakChannel::ChangeScreen(WiiSpeakScreenName name)
{
    screen.reset();
    if (name == WiiSpeakScreenName::ChatRoom)
        screen = std::make_unique<ChatRoomScreen>(session);
    else if (name == WiiSpeakScreenName::SettingsMenu)
        screen = std::make_unique<SettingsMenuScreen>(session);
    else if (name == WiiSpeakScreenName::Sensitivity)
        screen = std::make_unique<SensitivityScreen>(session);
    else if (name == WiiSpeakScreenName::Headphones)
        screen = std::make_unique<HeadphoneScreen>(session);
    else if (name == WiiSpeakScreenName::PerformanceCheck)
        screen = std::make_unique<PerformanceCheckScreen>(session);
    else if (name == WiiSpeakScreenName::Precautions)
        screen = std::make_unique<PrecautionsScreen>(session, WiiSpeakScreenName::SettingsMenu);
    else
        screen = std::make_unique<CreateMessageScreen>(session, miiCount);

    screenName = name;
}

}
