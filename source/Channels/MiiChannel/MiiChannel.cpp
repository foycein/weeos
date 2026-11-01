#include "Channels/MiiChannel/MiiChannel.h"

#include "Mii/MiiCreateId.h"

#include "Formats/U8Archive.h"

#include <chrono>

namespace Channels
{

namespace
{

// The music of the Mii Channel, the one sequence of NigaoeSound.brsar that plays on the player of the music (80018A2C). It goes on until the
// channel ends.
constexpr std::string_view Music = "SEQ_TMP_BGM";

// The channel's archive has two sounds for its keyboard: the focus of a key, and a press for everything else the keyboard does
// (Assumed, CHN-75).
constexpr std::string_view KeyboardFocusSound = "SEQ_KEYBOARD_FORCUS";
constexpr std::string_view KeyboardPushSound = "SEQ_KEYBOARD_PUSH";

// The words of the keyboard's buttons: Cancel and OK of the channel's messages, Back on the symbol window; the others keep the
// English words of sofkeybdkr.szs.
constexpr std::uint32_t BackWord = 301200;

std::uint32_t SystemIdOf(const Ios::NetworkConfigService& network)
{
    return Mii::MakeSystemId(network.GetWirelessMacAddress());
}

SystemMenu::KeyboardButtonTexts KeyboardTextsOf(const Formats::NumberedMessageFile& messages)
{
    SystemMenu::KeyboardButtonTexts texts;
    texts.cancel = MessageText(messages, MiiMessage::Cancel);
    texts.confirm = MessageText(messages, MiiMessage::Ok);
    texts.close = MessageText(messages, BackWord);
    return texts;
}

std::unique_ptr<Layout::LayoutArchive> ArchiveOf(const Formats::Bytes& bytes)
{
    return std::make_unique<Layout::LayoutArchive>(Formats::ReadU8Archive(bytes));
}

// The original seeds its chances with the console's clock, so a console whose clock stands still always shows the same plaza.
std::uint32_t SeedFromClock(const ChannelServices& services)
{
    return static_cast<std::uint32_t>(services.consoleTime().time_since_epoch().count());
}

}

MiiChannel::MiiChannel(ChannelServices& channelServices)
    : services(channelServices)
    , files(ReadMiiChannelFiles(channelServices.contents))
    , data(channelServices.storage.fileSystem, channelServices.storage.fileSystemCaller, SystemIdOf(channelServices.network))
    , figures(channelServices.display.Device(), files.bodies, data.ConsoleSystemId())
    , ground(channelServices.display.Device(), files.ground)
    , road(channelServices.display.Device(), files.road)
    , message(channelServices, *files.layouts, files.messages)
    , profile(channelServices, *files.layouts, files.messages)
    , keyboard(channelServices.display, ArchiveOf(files.keyboardArchive), KeyboardTextsOf(files.messages), channelServices.clock,
          channelServices.input, keyboardSounds)
    , plaza(channelServices, *files.layouts, files.messages, figures, ground, *files.faceParts, message, data, SeedFromClock(channelServices))
    , parade(channelServices, *files.layouts, files.messages, figures, road, *files.faceParts, message, data, SeedFromClock(channelServices))
    , lookAlike(channelServices, *files.layouts, files.messages, figures, *files.faceParts, message, SeedFromClock(channelServices))
    , editor(channelServices, *files.layouts, files.messages, figures, *files.faceParts, message, profile, keyboard, data)
    , hands(std::make_unique<SystemMenu::CursorManager>(channelServices.display, ArchiveOf(files.handArchive)))
{
    Graphics::GraphicsDevice& device = services.display.Device();
    fonts.AddFont(device, files.mainFont, {"newR_font_32_I4.brfnt", "WiiBitmapFontType1.brfnt"});
    fonts.AddFont(device, files.nameFont, {"nigaoe_font_32_I4.brfnt"});
    services.display.SetTextFonts(fonts.Fonts());

    services.sounds.Request(Music);
    StartFirstScene();
}

void MiiChannel::Update()
{
    // The channel starts black and fades in, as every channel does after its start.
    SystemMenu::Fader& fader = services.fader;
    if (fader.GetStatus() == SystemMenu::FaderStatus::Covered && !services.wiiMenu.IsRequested())
        fader.StartFadeIn();

    bool acceptsInput = !message.IsOpen() && !profile.IsOpen();
    if (message.IsOpen())
        message.Update();
    else if (profile.IsOpen())
        profile.Update();

    UpdateScene(acceptsInput);
    PlayKeyboardSounds();
}

// The keyboard asks for the menu's sounds by their names; the channel plays its own two instead.
void MiiChannel::PlayKeyboardSounds()
{
    for (const std::string& soundName : keyboardSounds.RequestedInThisStep())
    {
        if (soundName == SystemMenu::KeyboardSoundName::KeyFocused)
            services.sounds.Request(KeyboardFocusSound);
        else
            services.sounds.Request(KeyboardPushSound);
    }
    keyboardSounds.StartNextStep();
}

void MiiChannel::Draw()
{
    switch (scene)
    {
    case Scene::Starting:
    case Scene::Plaza:
    case Scene::NewMiiQuestions:
        plaza.Draw();
        break;
    case Scene::LookAlike:
        lookAlike.Draw();
        break;
    case Scene::Editor:
        editor.Draw();
        break;
    case Scene::Parade:
        parade.Draw();
        break;
    }

    profile.Draw();
    message.Draw();
}

SystemMenu::CursorManager& MiiChannel::Hands()
{
    return *hands;
}

MiiChannel::Scene MiiChannel::CurrentScene() const
{
    return scene;
}

PlazaScreen& MiiChannel::Plaza()
{
    return plaza;
}

ParadeScreen& MiiChannel::Parade()
{
    return parade;
}

EditorScreen& MiiChannel::Editor()
{
    return editor;
}

LookAlikeScreen& MiiChannel::LookAlike()
{
    return lookAlike;
}

MessageWindow& MiiChannel::Message()
{
    return message;
}

ProfileDialog& MiiChannel::Profile()
{
    return profile;
}

MiiChannelData& MiiChannel::Data()
{
    return data;
}

// A broken database asks to be formatted first (message 35, 8006457C); the first start greets the user and makes the first Mii.
void MiiChannel::StartFirstScene()
{
    plaza.Start();
    scene = Scene::Starting;

    if (data.State() == DatabaseState::Broken)
    {
        welcome = Welcome::BrokenDatabase;
        message.AskQuestion(MiiMessage::BrokenDatabase);
        return;
    }

    if (data.IsFirstStart())
    {
        welcome = Welcome::Greeting;
        message.ShowNotice(MiiMessage::Welcome);
        return;
    }

    scene = Scene::Plaza;
}

void MiiChannel::UpdateScene(bool acceptsInput)
{
    switch (scene)
    {
    case Scene::Starting:
        plaza.Update(false);
        FollowWelcome();
        return;
    case Scene::Plaza:
        plaza.Update(acceptsInput);
        FollowPlaza();
        return;
    case Scene::NewMiiQuestions:
        plaza.Update(false);
        FollowNewMiiQuestions();
        return;
    case Scene::LookAlike:
        lookAlike.Update(acceptsInput);
        FollowLookAlike();
        return;
    case Scene::Editor:
        editor.Update(acceptsInput);
        FollowEditor();
        return;
    case Scene::Parade:
        parade.Update(acceptsInput);
        FollowParade();
        return;
    }
}

void MiiChannel::FollowWelcome()
{
    std::optional<MessageAnswer> answer = message.TakeAnswer();
    if (!answer)
        return;

    switch (welcome)
    {
    case Welcome::BrokenDatabase:
        if (*answer != MessageAnswer::Yes)
        {
            services.wiiMenu.Request();
            return;
        }
        data.Format();
        plaza.Start();
        welcome = Welcome::None;
        if (data.IsFirstStart())
        {
            welcome = Welcome::Greeting;
            message.ShowNotice(MiiMessage::Welcome);
            return;
        }
        ShowPlaza();
        return;
    case Welcome::Greeting:
        welcome = Welcome::Explaining;
        message.ShowNotice(MiiMessage::MakeYourOwnMii);
        return;
    case Welcome::Explaining:
        welcome = Welcome::None;
        data.RememberStarted();
        isMakingFirstMii = true;
        AskNewMiiQuestion(ProfileQuestion::Gender);
        return;
    case Welcome::ThereIsYourMii:
        welcome = Welcome::HelpAndHomeButton;
        message.ShowNotice(MiiMessage::HelpAndHomeButton);
        return;
    case Welcome::HelpAndHomeButton:
        welcome = Welcome::None;
        ShowPlaza();
        return;
    case Welcome::None:
        return;
    }
}

void MiiChannel::FollowPlaza()
{
    std::optional<PlazaRequest> request = plaza.TakeRequest();
    if (!request)
        return;

    if (*request == PlazaRequest::MakeNewMii)
    {
        isMakingFirstMii = false;
        AskNewMiiQuestion(ProfileQuestion::Gender);
        return;
    }

    if (*request == PlazaRequest::ShowParade)
    {
        parade.Start();
        scene = Scene::Parade;
        return;
    }

    std::optional<std::size_t> slot = plaza.MiiToEdit();
    std::optional<Mii::MiiRecord> record = slot ? data.FindMii(*slot) : std::nullopt;
    if (!record)
        return;

    editor.Open(*record, slot);
    scene = Scene::Editor;
}

// "Mii Plaza" takes the user back; the Miis brought back from the parade are on the plaza then.
void MiiChannel::FollowParade()
{
    if (!parade.TakeWayBackToThePlaza())
        return;

    plaza.Start();
    scene = Scene::Plaza;
}

// Gender first, then the way to start; Cancel leaves the questions, Back goes back to the gender.
void MiiChannel::FollowNewMiiQuestions()
{
    std::optional<ProfileAnswer> answer = profile.TakeAnswer();
    if (!answer || !askedQuestion)
        return;

    ProfileQuestion question = *askedQuestion;
    askedQuestion.reset();

    if (question == ProfileQuestion::Gender)
    {
        if (answer->wasCancelled)
        {
            ShowPlaza();
            return;
        }
        newMiiGender = answer->choice == 1 ? Mii::Gender::Female : Mii::Gender::Male;
        AskNewMiiQuestion(ProfileQuestion::HowToMake);
        return;
    }

    if (answer->wasCancelled)
    {
        AskNewMiiQuestion(ProfileQuestion::Gender);
        return;
    }

    if (answer->choice == 0)
    {
        editor.Open(AsNewMiiOfThisConsole(MakeScratchMii(newMiiGender)), std::nullopt);
        scene = Scene::Editor;
        return;
    }

    lookAlike.Start(newMiiGender);
    scene = Scene::LookAlike;
}

// The face of a look-alike becomes a new Mii without names, and like the templates of "Start from scratch" it may not travel
// (Assumed, CHN-69).
void MiiChannel::FollowLookAlike()
{
    std::optional<LookAlikeResult> result = lookAlike.TakeResult();
    if (!result)
        return;

    if (result->wasCancelled)
    {
        scene = Scene::NewMiiQuestions;
        AskNewMiiQuestion(ProfileQuestion::HowToMake);
        return;
    }

    Mii::MiiRecord record = result->record;
    record.name.clear();
    record.creatorName.clear();
    record.mayNotTravel = true;
    editor.Open(AsNewMiiOfThisConsole(record), std::nullopt);
    scene = Scene::Editor;
}

void MiiChannel::FollowEditor()
{
    std::optional<EditorResult> result = editor.TakeResult();
    if (!result)
        return;

    if (result->wasSaved && result->slot)
        plaza.ShowSavedMii(*result->slot);

    if (isMakingFirstMii && result->wasSaved)
    {
        isMakingFirstMii = false;
        scene = Scene::Starting;
        welcome = Welcome::ThereIsYourMii;
        message.ShowNotice(MiiMessage::ThereIsYourMii);
        return;
    }

    isMakingFirstMii = false;
    ShowPlaza();
}

// A new Mii gets its create id when it is made: the time in four-second steps and the system id of this console (docs/research/Mii.md 1.4),
// so the editor shows it with the gray pants of an own Mii.
Mii::MiiRecord MiiChannel::AsNewMiiOfThisConsole(Mii::MiiRecord record) const
{
    std::chrono::sys_seconds now(services.consoleTime().time_since_epoch());
    record.miiId = Mii::MakeMiiId(now);
    record.systemId = data.ConsoleSystemId();
    return record;
}

void MiiChannel::AskNewMiiQuestion(ProfileQuestion question)
{
    askedQuestion = question;
    profile.Ask(question, true);
    scene = Scene::NewMiiQuestions;
}

void MiiChannel::ShowPlaza()
{
    scene = Scene::Plaza;
}

}
