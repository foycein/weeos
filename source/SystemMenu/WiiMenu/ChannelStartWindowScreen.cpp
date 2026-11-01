#include "SystemMenu/WiiMenu/ChannelStartWindowScreen.h"

#include "SystemMenu/DialogManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/HomeMenu/HomeButton.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuError.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MenuSettings.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/TitleLaunchRequest.h"
#include "SystemMenu/WiiMenu/ChannelGridScreen.h"
#include "SystemMenu/WiiMenu/ChannelTile.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"
#include "SystemMenu/ZoomedWindow.h"

#include "Formats/FormatError.h"
#include "Text/MessageCatalog.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view WindowBundle = "chanTtl";
constexpr std::string_view DiscBundle = "diskBann";
constexpr std::string_view FrameLayout = "my_ChTop_a";
constexpr std::string_view DiscLayout = "my_DiskCh_a";
constexpr std::string_view BannerLayout = "banner";
constexpr std::string_view RootPaneName = "RootPane";

// The left button goes back to the Wii Menu, the right one starts the channel.
constexpr std::size_t WiiMenuButton = 0;
constexpr std::size_t StartButton = 1;

constexpr std::string_view FocusOnAnimation = "my_ChTop_a_FocusBtn_on";
constexpr std::string_view FocusOffAnimation = "my_ChTop_a_FocusBtnA_off";
constexpr std::string_view PressAnimation = "my_ChTop_a_SelectBtn_Ac";
constexpr std::string_view GrayAnimation = "my_ChTop_a_OffBtn";

// The messages of the window (ipl_common.bmg).
constexpr std::uint16_t DiscChannelMessage = 0;
constexpr std::uint16_t WiiMenuMessage = 1;
constexpr std::uint16_t StartMessage = 2;
constexpr std::uint16_t InsertDiscMessage = 4;
constexpr std::uint16_t CannotReadDiscMessage = 6;
constexpr std::uint16_t CannotBeUsedMessage = 19;
constexpr std::uint16_t UpdateMessage = 20;
constexpr std::uint16_t QuitMessage = 37;
constexpr std::uint16_t OkMessage = 46;
constexpr std::uint16_t InternetRequiredMessage = 323;
constexpr std::uint16_t EnterSettingsMessage = 326;

// The banner of a channel and of the Disc Channel.
constexpr std::string_view BannerFilePath = "meta/banner.bin";
constexpr std::string_view SoundFilePath = "meta/sound.bin";
constexpr std::string_view DiscStartAnimation = "my_DiskCh_a_Start";
constexpr std::string_view NoDiscSound = "WIPL_ME_NO_DISC_BANNER";

constexpr std::string_view FocusSound = "WIPL_SE_BT_TARGETTING";
constexpr std::string_view PushSound = "WIPL_SE_BT_PUSH";
constexpr std::string_view DecideSound = "WIPL_SE_DECIDE";
constexpr std::string_view GraySound = "WIPL_SE_GRAY_BUTTON";
constexpr std::string_view CloseSound = "WIPL_SE_CH_UNSELECT";

// The pages of Wii Settings that the window leads to (SystemMenuStructure.md 3.4): the Internet settings and the system update.
constexpr int InternetSettingsPage = 1;
constexpr int SystemUpdatePage = 3;

// The password dialog says, after a right PIN, that the restrictions are lifted until the user is back at the Wii Menu.
constexpr int PinDialogArgument = 0;

// The original waits 100 ms after Start before it fades out (0x813B9AC0: more than 100 / 16.68 steps).
constexpr int StepsBeforeTheFadeToStart = 6;

}

ChannelStartWindowScreen::ChannelStartWindowScreen(MenuServices& menuServices)
    : Screen(ScreenId::ChannelStartWindow)
    , services(menuServices)
{
}

StartWindowState ChannelStartWindowScreen::State() const
{
    return state;
}

bool ChannelStartWindowScreen::CanStart() const
{
    return !isDiscChannel;
}

const std::u16string& ChannelStartWindowScreen::StartButtonText() const
{
    return startButtonText;
}

bool ChannelStartWindowScreen::UpdatesWithChildren() const
{
    return true;
}

bool ChannelStartWindowScreen::DrawsWithChildren() const
{
    return true;
}

void ChannelStartWindowScreen::Load()
{
    slot = services.wiiMenu.pressedChannel.value_or(SlotPosition{});
    windowArchive = services.files.OpenLayoutBundle(WindowBundle);
    discArchive = services.files.OpenLayoutBundle(DiscBundle);
    ReadTheChannel();
}

// The Disc Channel shows the empty drive (SystemMenuStructure.md 4.4): WeeOS has no disc drive with a disc in it.
void ChannelStartWindowScreen::ReadTheChannel()
{
    const SavedSlot& savedSlot = services.wiiMenu.channels.SlotAt(slot);
    isDiscChannel = savedSlot.kind == SavedSlotKind::DiscChannel;
    if (isDiscChannel)
        return;

    titleId = savedSlot.titleId;
    std::optional<InstalledTitle> installedTitle = FindInstalledTitle(services.wiiMenu.console, titleId);
    if (installedTitle)
    {
        isOldVersion = IsOldVersion(titleId, installedTitle->version);
        makerCode = installedTitle->groupId;
    }

    try
    {
        ReadTheBanner();
    }
    catch (const Formats::FormatError& error)
    {
        throw MenuError("The banner of the title " + titleId.ToString() + " is damaged: " + error.what());
    }
}

// A channel whose banner cannot be read ends the menu with the error screen of corrupted system files, as the original does when it
// cannot read the title (0x813BA568).
void ChannelStartWindowScreen::ReadTheBanner()
{
    banner = Formats::ReadBannerContent(services.wiiMenu.console.ReadBannerContent(titleId));
    std::optional<Formats::ByteView> bannerFile = banner->files.FindFile(BannerFilePath);
    if (!bannerFile)
        throw MenuError("The banner content of the title " + titleId.ToString() + " has no banner");

    Formats::U8Archive bannerFiles = Formats::ReadLayoutFile(*bannerFile);
    bannerAnimations = ChooseBannerAnimations(bannerFiles, banner->isBannerDrivenByCode);
    bannerArchive = std::make_unique<Layout::LayoutArchive>(std::move(bannerFiles));
}

void ChannelStartWindowScreen::Build()
{
    BuildTheFrame();
    if (isDiscChannel)
        BuildTheDiscBanner();
    else
        BuildTheBanner();
}

void ChannelStartWindowScreen::BuildTheFrame()
{
    frame = std::make_unique<MenuLayout>(services.display, *windowArchive, std::string(FrameLayout));
    startButtonText = services.messages.Find(isOldVersion ? UpdateMessage : StartMessage);
    frame->SetText("T_BtnA", services.messages.Find(WiiMenuMessage));
    frame->SetText("T_BtnB", startButtonText);

    focusOffOfWiiMenu = frame->BindAnimation(std::string(FocusOffAnimation), "G_FocusBtnA", Layout::PlaybackMode::PlayOnce);
    focusOnOfWiiMenu = frame->BindAnimation(std::string(FocusOnAnimation), "G_FocusBtnA", Layout::PlaybackMode::PlayOnce);
    focusOffOfStart = frame->BindAnimation(std::string(FocusOffAnimation), "G_FocusBtnB", Layout::PlaybackMode::PlayOnce);
    focusOnOfStart = frame->BindAnimation(std::string(FocusOnAnimation), "G_FocusBtnB", Layout::PlaybackMode::PlayOnce);
    pressOfWiiMenu = frame->BindAnimation(std::string(PressAnimation), "G_SelectBtnA", Layout::PlaybackMode::PlayOnce);
    pressOfStart = frame->BindAnimation(std::string(PressAnimation), "G_SelectBtnB", Layout::PlaybackMode::PlayOnce);

    if (!CanStart())
    {
        std::size_t gray = frame->BindAnimation(std::string(GrayAnimation), "G_OnOffBtnB", Layout::PlaybackMode::PlayOnce);
        frame->ShowLastFrameOfAnimation(gray);
    }

    buttons = std::make_unique<ButtonGroup>(*frame, services.display, std::vector<std::string>{"B_BtnA", "B_BtnB"});
}

// A banner without a script plays banner_Start once and then banner_Loop, or banner over and over. A banner that a script or a module
// drives shows its English panes and what the script shows while the channel has no data of its own: its start, or its first animation
// on the group Rso0 (docs/research/Channels.md 1.6, the same choice as for the icons, SYS-85).
void ChannelStartWindowScreen::BuildTheBanner()
{
    bannerLayout = std::make_unique<MenuLayout>(services.display, *bannerArchive, std::string(BannerLayout));
    const BannerAnimations& animations = bannerAnimations;
    if (banner->isBannerDrivenByCode)
        ShowOnlyTheEnglishPanes(*bannerLayout, *bannerArchive, std::string(BannerLayout));

    if (animations.start)
        bannerStart = bannerLayout->BindAnimationToPane(*animations.start, RootPaneName, Layout::PlaybackMode::PlayOnce);

    if (animations.loop && animations.loopsOnTheScriptGroup)
        bannerLoop = bannerLayout->BindAnimation(*animations.loop, ScriptGroupOfBanners, Layout::PlaybackMode::Loop);
    else if (animations.loop)
        bannerLoop = bannerLayout->BindAnimationToPane(*animations.loop, RootPaneName, Layout::PlaybackMode::Loop);
}

void ChannelStartWindowScreen::BuildTheDiscBanner()
{
    bannerLayout = std::make_unique<MenuLayout>(services.display, *discArchive, std::string(DiscLayout));
    bannerLayout->SetText("T_Bar", services.messages.Find(DiscChannelMessage));
    bannerLayout->SetText("T_Comment0", services.messages.Find(InsertDiscMessage));
    bannerLayout->SetText("T_Comment1", services.messages.Find(CannotReadDiscMessage));
    bannerStart = bannerLayout->BindAnimationToAllPanes(std::string(DiscStartAnimation));
}

void ChannelStartWindowScreen::BeforeStep()
{
    frame->Calculate();
    bannerLayout->Calculate();

    bool hasStartEnded = !bannerStart || !bannerLayout->IsAnimationPlaying(*bannerStart);
    bool isShown = state != StartWindowState::WaitingForTheZoom && state != StartWindowState::ZoomingIn;
    if (isShown && hasStartEnded && bannerLoop && !isBannerLooping)
    {
        bannerLayout->StartAnimation(*bannerLoop);
        isBannerLooping = true;
    }
}

// The original waits until the grid has zoomed in and the window has faded in inside the television (states 2 and 3).
bool ChannelStartWindowScreen::IsReady()
{
    ChannelGridScreen* grid = Grid();
    if (state == StartWindowState::WaitingForTheZoom)
    {
        if (grid != nullptr)
            grid->ZoomIntoPressedChannel();

        state = StartWindowState::ZoomingIn;
        return false;
    }

    bool isZooming = grid != nullptr && grid->Zoom() != nullptr && grid->Zoom()->IsMoving();
    return !isZooming;
}

void ChannelStartWindowScreen::OnStart()
{
    StartTheBanner();
    state = StartWindowState::ShowingChannel;
}

// The banner and its sound start when the window has faded in (0x813B80D4, 0x813B81CC). The Disc Channel without a disc plays its own
// jingle (0x813B8644).
void ChannelStartWindowScreen::StartTheBanner()
{
    if (bannerStart)
        bannerLayout->StartAnimation(*bannerStart);

    if (isDiscChannel)
    {
        services.sounds.Request(NoDiscSound);
        return;
    }

    std::optional<Formats::ByteView> soundFile = banner->files.FindFile(SoundFilePath);
    if (soundFile)
        services.sounds.StartBannerSound(*soundFile);
}

bool ChannelStartWindowScreen::RunMain()
{
    switch (state)
    {
    case StartWindowState::ShowingChannel:
        RunShowingChannel();
        break;

    case StartWindowState::ShowingCannotBeUsed:
        ReadAnswerOfCannotBeUsed();
        break;

    case StartWindowState::AskingForInternetSettings:
        ReadAnswerOfInternetQuestion();
        break;

    case StartWindowState::WaitingForPin:
        ReadAnswerOfPinDialog();
        break;

    case StartWindowState::Starting:
        RunStarting();
        break;

    case StartWindowState::FadingOutToStart:
        RunFadingOutToStart();
        break;

    case StartWindowState::LeavingForSettings:
        if (services.fader.GetStatus() == FaderStatus::Covered)
        {
            GoToRoot(ScreenId::WiiSettings, settingsPageToOpen);
            return true;
        }
        break;

    case StartWindowState::ZoomingOut:
        return Grid() == nullptr || Grid()->Zoom() == nullptr;

    default:
        break;
    }
    return false;
}

void ChannelStartWindowScreen::RunShowingChannel()
{
    bool isActive = services.fader.GetStatus() == FaderStatus::Clear;
    for (const ButtonGroupEvent& event : buttons->Update(services.input, isActive))
        ReactToButton(event);
}

void ChannelStartWindowScreen::ReactToButton(const ButtonGroupEvent& event)
{
    bool isWiiMenu = event.buttonIndex == WiiMenuButton;
    switch (event.kind)
    {
    case ButtonGroupEventKind::Focused:
        frame->StartAnimation(isWiiMenu ? focusOnOfWiiMenu : focusOnOfStart);
        services.sounds.Request(FocusSound);
        break;

    case ButtonGroupEventKind::Unfocused:
        frame->StartAnimation(isWiiMenu ? focusOffOfWiiMenu : focusOffOfStart);
        break;

    case ButtonGroupEventKind::Pressed:
        if (isWiiMenu)
            CloseTowardsTheGrid();
        else
            PressStart();
        break;
    }
}

// What Start does depends on the channel (0x813B9488 and 0x813B6F10): an old channel asks for the system update, a channel that needs the
// Internet asks for the Internet settings while no connection has passed its test, a restricted channel asks for the PIN.
void ChannelStartWindowScreen::PressStart()
{
    if (!CanStart())
    {
        services.sounds.Request(GraySound);
        return;
    }

    frame->StartAnimation(pressOfStart);
    services.sounds.Request(DecideSound);

    if (isOldVersion)
    {
        LeaveForSettings(SystemUpdatePage);
        return;
    }

    if (!services.titleLaunch.HasProgram(titleId))
    {
        ShowMessage(CannotBeUsedMessage);
        return;
    }

    if (banner->needsInternetConnection && !services.wiiMenu.console.HasTestedInternetConnection())
    {
        AskForInternetSettings();
        return;
    }

    if (IsRestrictedByParentalControls())
    {
        services.parentalAnswer = ParentalAnswer::None;
        services.screens.OpenChild(ScreenId::ParentalDialog, ScreenId::ChannelStartWindow, PinDialogArgument);
        state = StartWindowState::WaitingForPin;
        return;
    }

    BeginStart();
}

// WeeOS shows the menu's own message for a channel that it has no program for yet (DECISIONS.md), in a dialog instead of the error screen
// of the original, so that the user can go back.
void ChannelStartWindowScreen::ShowMessage(std::uint16_t messageNumber)
{
    DialogRequest request;
    request.layout = DialogLayout::OneButton;
    request.message = services.messages.Find(messageNumber);
    request.buttonBText = services.messages.Find(OkMessage);
    services.dialogs.Open(request);
    state = StartWindowState::ShowingCannotBeUsed;
}

void ChannelStartWindowScreen::AskForInternetSettings()
{
    DialogRequest request;
    request.layout = DialogLayout::TwoButtons;
    request.message = services.messages.Find(InternetRequiredMessage);
    request.buttonAText = services.messages.Find(EnterSettingsMessage);
    request.buttonBText = services.messages.Find(QuitMessage);
    services.dialogs.Open(request);
    state = StartWindowState::AskingForInternetSettings;
}

void ChannelStartWindowScreen::ReadAnswerOfCannotBeUsed()
{
    if (services.dialogs.TakeAnswer())
        state = StartWindowState::ShowingChannel;
}

void ChannelStartWindowScreen::ReadAnswerOfInternetQuestion()
{
    std::optional<DialogAnswer> answer = services.dialogs.TakeAnswer();
    if (!answer)
        return;

    if (answer == DialogAnswer::ButtonA)
        LeaveForSettings(InternetSettingsPage);
    else
        state = StartWindowState::ShowingChannel;
}

void ChannelStartWindowScreen::ReadAnswerOfPinDialog()
{
    if (HasChildren() || services.screens.IsLoadingScreen())
        return;

    if (services.parentalAnswer == ParentalAnswer::PinCorrect)
        BeginStart();
    else
        state = StartWindowState::ShowingChannel;
}

// The channel starts (0x813B97FC): HOME does nothing from now on, the menu remembers the channel, and a moment later the picture fades.
void ChannelStartWindowScreen::BeginStart()
{
    services.homeButton.Disable();
    services.wiiMenu.channels.RememberChannelThatStarts(slot);
    stepsWaited = 0;
    state = StartWindowState::Starting;
}

void ChannelStartWindowScreen::RunStarting()
{
    ++stepsWaited;
    if (stepsWaited < StepsBeforeTheFadeToStart)
        return;

    services.fader.StartFadeOut();
    services.sounds.StopBannerSound();
    state = StartWindowState::FadingOutToStart;
}

// When the picture is black the play record is written and the console is asked to start the title (0x813B51F0).
void ChannelStartWindowScreen::RunFadingOutToStart()
{
    if (services.fader.GetStatus() != FaderStatus::Covered)
        return;

    LocalTime now = services.wiiMenu.console.GetLocalTime();
    const Formats::ChannelName& name = banner->names[Formats::EnglishLanguageNumber];
    services.wiiMenu.console.WritePlayRecord(MakePlayRecord(name, titleId, makerCode, now));
    services.titleLaunch.Request(titleId);
    state = StartWindowState::Started;
}

void ChannelStartWindowScreen::LeaveForSettings(int settingsPage)
{
    settingsPageToOpen = settingsPage;
    services.fader.StartFadeOut();
    services.sounds.StopBannerSound();
    state = StartWindowState::LeavingForSettings;
}

// Wii Menu: the button flashes, the grid zooms out and the window fades away inside the television (0x813B76D8).
void ChannelStartWindowScreen::CloseTowardsTheGrid()
{
    frame->StartAnimation(pressOfWiiMenu);
    services.sounds.Request(PushSound);
    services.sounds.Request(CloseSound);
    services.sounds.StopBannerSound();

    ChannelGridScreen* grid = Grid();
    if (grid != nullptr)
        grid->ZoomOutOfPressedChannel();

    state = StartWindowState::ZoomingOut;
}

// The parental controls restrict the News Channel and the Internet Channel when the settings say so (0x813BAB50). The age ratings of
// games are left out: WeeOS has no games.
bool ChannelStartWindowScreen::IsRestrictedByParentalControls()
{
    std::optional<ParentalControl> parentalControl = services.settings.ReadParentalControl();
    if (!parentalControl || !parentalControl->isOn)
        return false;

    if (IsNewsChannel(titleId))
        return services.settings.IsNewsChannelRestricted();

    if (IsInternetChannel(titleId))
        return services.settings.IsInternetChannelRestricted();

    return false;
}

ChannelGridScreen* ChannelStartWindowScreen::Grid()
{
    return dynamic_cast<ChannelGridScreen*>(services.screens.FindScreen(ScreenId::ChannelGrid));
}

// The original draws nothing before the zoom starts. While the grid zooms, the window is a picture inside the television, with black
// around it (0x813B5A54, pass 0 and pass 1).
void ChannelStartWindowScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main || state == StartWindowState::WaitingForTheZoom)
        return;

    if (state == StartWindowState::ZoomingIn || state == StartWindowState::ZoomingOut)
    {
        DrawTheWindowInsideTheTelevision();
        return;
    }

    DrawTheWindow();
}

void ChannelStartWindowScreen::DrawTheWindow()
{
    bannerLayout->Draw();
    frame->Draw();
}

void ChannelStartWindowScreen::DrawTheWindowInsideTheTelevision()
{
    ChannelGridScreen* grid = Grid();
    if (grid == nullptr || grid->Zoom() == nullptr)
        return;

    DrawWindowInsideTheTelevision(services.display, *grid->Zoom(), [this] { DrawTheWindow(); });
}

}
