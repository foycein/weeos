#include "SystemMenu/WiiMenu/ChannelStartWindowScreen.h"

#include "SystemMenu/DialogManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/HomeMenu/HomeButton.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuError.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MessageBoard/BoardInput.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/TitleLaunchRequest.h"
#include "SystemMenu/WiiMenu/BottomBarScreen.h"
#include "SystemMenu/WiiMenu/ChannelGridScreen.h"
#include "SystemMenu/WiiMenu/ChannelTile.h"
#include "SystemMenu/WiiMenu/ChannelTitleIds.h"
#include "SystemMenu/WiiMenu/LayoutParts.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"

#include "Formats/FormatError.h"
#include "Text/MessageCatalog.h"

#include <array>

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
constexpr std::string_view BrightAnimation = "my_ChTop_a_OnBtn";

// The change to another channel plays on the pane Change (0x813B4374): ChangeIn while the old channel goes, ChangeOut while the new one
// comes. The original plays ChangeRoop between them while the new banner still loads; WeeOS reads it at once, so the loop never plays.
constexpr std::string_view ChangePane = "Change";
constexpr std::string_view ChangeInAnimation = "my_ChTop_a_ChangeIn";
constexpr std::string_view ChangeOutAnimation = "my_ChTop_a_ChangeOut";

// On a wide screen the frame around the banner takes the textures of three pictures that the layout keeps for it (0x813B4374): the
// corners, the sides, and the edges at the top and at the bottom.
struct WideFramePart
{
    std::string_view framePicture;
    std::string_view widePicture;
};

constexpr std::array<WideFramePart, 12> WideFrameParts = {{
    {"Fre_a", "Picture_04"},
    {"Fre_d", "Picture_04"},
    {"Fre_i", "Picture_04"},
    {"Fre_l", "Picture_04"},
    {"Fre_e", "Picture_05"},
    {"Fre_f", "Picture_05"},
    {"Fre_g", "Picture_05"},
    {"Fre_h", "Picture_05"},
    {"Fre_b", "Picture_06"},
    {"Fre_c", "Picture_06"},
    {"Fre_j", "Picture_06"},
    {"Fre_k", "Picture_06"},
}};

// Plus changes to the next channel and Minus to the one before; on the Classic Controller also R and L, the buttons that turn the pages
// of the grid (0x813B654C). A on an arrow of the bar does the same (0x813BAF1C).
constexpr ButtonSet NextChannelButtons = {Input::RemoteButtons::Plus, Input::ClassicButtons::Plus | Input::ClassicButtons::RightTrigger};
constexpr ButtonSet PreviousChannelButtons = {Input::RemoteButtons::Minus, Input::ClassicButtons::Minus | Input::ClassicButtons::LeftTrigger};
constexpr ButtonSet ArrowPressButtons = {Input::RemoteButtons::A, Input::ClassicButtons::A};
constexpr int NextChannel = 1;
constexpr int PreviousChannel = -1;

// The messages of the window (ipl_common.bmg).
constexpr std::uint16_t DiscChannelMessage = 0;
constexpr std::uint16_t WiiMenuMessage = 1;
constexpr std::uint16_t StartMessage = 2;
constexpr std::uint16_t InsertDiscMessage = 4;
constexpr std::uint16_t CannotReadDiscMessage = 6;
constexpr std::uint16_t CannotBeUsedMessage = 19;
constexpr std::uint16_t OkMessage = 46;

// The banner of a channel and of the Disc Channel.
constexpr std::string_view BannerFilePath = "meta/banner.bin";
constexpr std::string_view SoundFilePath = "meta/sound.bin";
constexpr std::string_view DiscStartAnimation = "my_DiskCh_a_Start";

// The empty drive shows the discs it takes. WeeOS takes no GameCube disc, so its disc, shade, reflection and
// window stay hidden.
constexpr std::array<std::string_view, 4> GameCubeDiscPanes = {"N_GC0", "ShadeGC", "N_RefGC", "W_GC"};
constexpr std::string_view NoDiscSound = "WIPL_ME_NO_DISC_BANNER";

// The scripts of two banners: the News Channel without save data shows the text textT0, the Photo
// Channel loops its first animation from this frame.
constexpr std::string_view NewsTextWithoutSaveData = "textT0";
constexpr float FirstFrameOfThePhotoBannerLoop = 40;

constexpr std::string_view FocusSound = "WIPL_SE_BT_TARGETTING";
constexpr std::string_view PushSound = "WIPL_SE_BT_PUSH";
constexpr std::string_view DecideSound = "WIPL_SE_DECIDE";
constexpr std::string_view GraySound = "WIPL_SE_GRAY_BUTTON";
constexpr std::string_view CloseSound = "WIPL_SE_CH_UNSELECT";
constexpr std::string_view ChangeSound = "WSD_SELECT";

// The original waits 100 ms after Start before it fades out (0x813B9AC0: more than 100 / 16.68 steps).
constexpr int StepsBeforeTheFadeToStart = 6;

// The channel that the window changes to (0x813B91B0): the next used place after this one, or the one before it, over all pages and
// round from the last place to the first. The place itself when no other place is used.
SlotPosition NeighborChannel(const ChannelManager& channels, SlotPosition from, int direction)
{
    int fromNumber = from.page * SlotsPerPage + from.index;
    int number = fromNumber;
    for (int tried = 1; tried < SlotCount; ++tried)
    {
        number += direction;
        if (number < 0)
            number = SlotCount - 1;
        if (number >= SlotCount)
            number = 0;

        SlotPosition position = {number / SlotsPerPage, number % SlotsPerPage};
        if (channels.SlotAt(position).kind != SavedSlotKind::Empty)
            return position;
    }
    return from;
}

}

ChannelStartWindowScreen::ChannelStartWindowScreen(MenuServices& menuServices)
    : Screen(ScreenId::ChannelStartWindow)
    , services(menuServices)
    , zoomedWindow(menuServices.display)
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
    makerCode = 0;
    banner.reset();
    if (isDiscChannel)
        return;

    titleId = savedSlot.titleId;
    std::optional<InstalledTitle> installedTitle = FindInstalledTitle(services.wiiMenu.console, titleId);
    if (installedTitle)
    {
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

// As soon as the window is built it asks the grid for the zoom (0x813B4374 ends with 0x813B7670).
void ChannelStartWindowScreen::Build()
{
    BuildTheFrame();
    BuildTheBanner();

    ChannelGridScreen* grid = Grid();
    if (grid != nullptr)
        grid->ZoomIntoPressedChannel();

    state = StartWindowState::ZoomingIn;
}

void ChannelStartWindowScreen::BuildTheFrame()
{
    frame = std::make_unique<MenuLayout>(services.display, *windowArchive, std::string(FrameLayout));
    startButtonText = services.messages.Find(StartMessage);
    frame->SetText("T_BtnA", services.messages.Find(WiiMenuMessage));
    frame->SetText("T_BtnB", startButtonText);

    focusOffOfWiiMenu = frame->BindAnimation(std::string(FocusOffAnimation), "G_FocusBtnA", Layout::PlaybackMode::PlayOnce);
    focusOnOfWiiMenu = frame->BindAnimation(std::string(FocusOnAnimation), "G_FocusBtnA", Layout::PlaybackMode::PlayOnce);
    focusOffOfStart = frame->BindAnimation(std::string(FocusOffAnimation), "G_FocusBtnB", Layout::PlaybackMode::PlayOnce);
    focusOnOfStart = frame->BindAnimation(std::string(FocusOnAnimation), "G_FocusBtnB", Layout::PlaybackMode::PlayOnce);
    pressOfWiiMenu = frame->BindAnimation(std::string(PressAnimation), "G_SelectBtnA", Layout::PlaybackMode::PlayOnce);
    pressOfStart = frame->BindAnimation(std::string(PressAnimation), "G_SelectBtnB", Layout::PlaybackMode::PlayOnce);

    grayStart = frame->BindAnimation(std::string(GrayAnimation), "G_OnOffBtnB", Layout::PlaybackMode::PlayOnce);
    brightStart = frame->BindAnimation(std::string(BrightAnimation), "G_OnOffBtnB", Layout::PlaybackMode::PlayOnce);
    if (!CanStart())
        frame->ShowLastFrameOfAnimation(grayStart);

    changeIn = frame->BindAnimationToPane(std::string(ChangeInAnimation), ChangePane, Layout::PlaybackMode::PlayOnce);
    changeOut = frame->BindAnimationToPane(std::string(ChangeOutAnimation), ChangePane, Layout::PlaybackMode::PlayOnce);

    UseTheWideFrame();

    buttons = std::make_unique<ButtonController>(*frame, services.display, std::vector<std::string>{"B_BtnA", "B_BtnB"});
}

void ChannelStartWindowScreen::UseTheWideFrame()
{
    for (const WideFramePart& part : WideFrameParts)
        CopyFirstTexture(*frame, part.framePicture, part.widePicture);
}

void ChannelStartWindowScreen::BuildTheBanner()
{
    hasBannerStarted = false;
    isBannerLooping = false;
    bannerStart.reset();
    bannerLoop.reset();
    bannerScript.reset();

    if (isDiscChannel)
        BuildTheDiscBanner();
    else
        BuildTheChannelBanner();
}

// Every banner shows its English panes (0x813B63EC calls 0x813B24C0). A banner without a script plays banner_Start once and then
// banner_Loop, or banner over and over. A banner that a script or a module drives shows what the script shows while the channel has
// no data of its own: its start, or its first animation on the group Rso0.
void ChannelStartWindowScreen::BuildTheChannelBanner()
{
    bannerLayout = std::make_unique<MenuLayout>(services.display, *bannerArchive, std::string(BannerLayout));
    const BannerAnimations& animations = bannerAnimations;
    ShowOnlyTheEnglishPanes(bannerLayout->Live(), bannerArchive->ReadLayout(std::string(BannerLayout)));

    if (animations.start)
        bannerStart = bannerLayout->BindAnimationToPane(*animations.start, RootPaneName, Layout::PlaybackMode::PlayOnce);

    if (animations.loop && animations.loopsOnTheScriptGroup)
        BuildTheBannerScript(*animations.loop);
    else if (animations.loop)
        bannerLoop = bannerLayout->BindAnimationToPane(*animations.loop, RootPaneName, Layout::PlaybackMode::Loop);
}

// The News Channel's script shows the text for a channel without save data.
void ChannelStartWindowScreen::BuildTheBannerScript(const std::string& animationName)
{
    std::vector<Formats::Pane*> panes = bannerLayout->Live().PanesOfGroup(ScriptGroupOfBanners);
    bannerScript = std::make_unique<FramePlayer>(*bannerLayout, *bannerArchive, animationName, panes);
    if (IsInFamily(titleId, SystemChannelKind, NewsChannelFamily))
        bannerLayout->PaneNamed(NewsTextWithoutSaveData).isVisible = true;
}

// What the script of a banner does with its first animation while the channel has no data of its own (Channels.md 1.6): the Photo
// Channel plays it and loops it from frame 40, the News Channel plays it once and holds its last frame, the module of the Forecast
// Channel, which is not decoded, loops it.
void ChannelStartWindowScreen::StartTheBannerScript()
{
    float lastFrame = bannerScript->LastFrameOfTheFile();
    if (IsInFamily(titleId, SystemChannelKind, PhotoChannel11Family))
        bannerScript->LoopFrom(FirstFrameOfThePhotoBannerLoop, lastFrame + 1, 0);
    else if (IsInFamily(titleId, SystemChannelKind, NewsChannelFamily))
        bannerScript->PlayOnce(0, lastFrame);
    else
        bannerScript->Loop(0, lastFrame + 1);
}

void ChannelStartWindowScreen::BuildTheDiscBanner()
{
    bannerLayout = std::make_unique<MenuLayout>(services.display, *discArchive, std::string(DiscLayout));
    bannerLayout->SetText("T_Bar", services.messages.Find(DiscChannelMessage));
    bannerLayout->SetText("T_Comment0", services.messages.Find(InsertDiscMessage));
    bannerLayout->SetText("T_Comment1", services.messages.Find(CannotReadDiscMessage));
    bannerStart = bannerLayout->BindAnimationToAllPanes(std::string(DiscStartAnimation));

    for (std::string_view paneName : GameCubeDiscPanes)
        bannerLayout->PaneNamed(paneName).isVisible = false;
}

// The loop of the banner follows its start (0x813B4D10).
void ChannelStartWindowScreen::BeforeStep()
{
    frame->Calculate();
    bannerLayout->Calculate();
    if (bannerScript)
        bannerScript->Step();

    bool hasStartEnded = !bannerStart || !bannerLayout->IsAnimationPlaying(*bannerStart);
    if (!hasBannerStarted || !hasStartEnded || isBannerLooping)
        return;

    if (bannerLoop)
        bannerLayout->StartAnimation(*bannerLoop);

    if (bannerScript)
        StartTheBannerScript();

    isBannerLooping = true;
}

// The original waits until the grid has zoomed in and the window has faded in inside the television (state 3).
bool ChannelStartWindowScreen::IsReady()
{
    return !IsTheGridZooming();
}

// The arrows of the bar come back, both of them, to change the channel (0x813B4EC4).
void ChannelStartWindowScreen::OnStart()
{
    StartTheBanner();
    BottomBarScreen* bar = Bar();
    if (bar != nullptr)
        bar->ShowPageArrows(true, true);

    state = StartWindowState::ShowingChannel;
}

// The banner and its sound start when the window has faded in (0x813B80D4, 0x813B81CC). The Disc Channel without a disc plays its own
// jingle (0x813B8644).
void ChannelStartWindowScreen::StartTheBanner()
{
    hasBannerStarted = true;
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

    case StartWindowState::LeavingTheOldChannel:
        RunLeavingTheOldChannel();
        break;

    case StartWindowState::ArrivingAtTheNewChannel:
        RunArrivingAtTheNewChannel();
        break;

    case StartWindowState::ShowingCannotBeUsed:
        ReadAnswerOfCannotBeUsed();
        break;

    case StartWindowState::Starting:
        RunStarting();
        break;

    case StartWindowState::FadingOutToStart:
        RunFadingOutToStart();
        break;

    default:
        break;
    }

    // Wii Menu ends the window in the step of the press (0x813B4F94, state 6).
    return state == StartWindowState::ZoomingOut;
}

// The window goes away while the grid zooms out (state 6): the arrows of the bar slide out (0x813B5130), and the window is gone when the
// zoom has ended (0x813B51F0).
void ChannelStartWindowScreen::OnRunEnded()
{
    BottomBarScreen* bar = Bar();
    if (bar != nullptr)
        bar->ShowPageArrows(false, false);
}

bool ChannelStartWindowScreen::RunClosing()
{
    return !IsTheGridZooming();
}

// Plus and Minus come first; the buttons of the window wait for the next step (0x813B654C).
void ChannelStartWindowScreen::RunShowingChannel()
{
    if (HandleChangeOfChannel())
        return;

    bool isActive = services.fader.GetStatus() == FaderStatus::Clear;
    for (const ButtonEvent& event : buttons->Poll(services.input, isActive))
        ReactToButton(event);
}

bool ChannelStartWindowScreen::HandleChangeOfChannel()
{
    std::optional<int> direction;
    if (services.input.AnyPlayerPressed(PreviousChannelButtons))
        direction = PreviousChannel;
    else if (services.input.AnyPlayerPressed(NextChannelButtons))
        direction = NextChannel;
    else
        direction = ChangeAskedForByTheArrows();

    if (!direction)
        return false;

    ChangeChannel(*direction);
    return true;
}

std::optional<int> ChannelStartWindowScreen::ChangeAskedForByTheArrows()
{
    BottomBarScreen* bar = Bar();
    if (bar == nullptr)
        return std::nullopt;

    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
        if (!player || !player->state.pointerIsValid)
            continue;

        if (!HasGoneDown(*player, ArrowPressButtons))
            continue;

        // The pressed arrow flashes: the window sends the bar the event 0x14 for the left arrow and 0x13 for the right one.
        std::optional<BarButton> button = bar->ButtonAt(services.display.PointerToCanvas(player->state.pointer));
        if (button == BarButton::PreviousPage)
        {
            bar->FlashPageArrow(BarButton::PreviousPage);
            return PreviousChannel;
        }

        if (button == BarButton::NextPage)
        {
            bar->FlashPageArrow(BarButton::NextPage);
            return NextChannel;
        }
    }
    return std::nullopt;
}

// The window changes to the next used place of the grid or the one before (0x813B9290). The grid will zoom out of the television of
// the new channel.
void ChannelStartWindowScreen::ChangeChannel(int direction)
{
    services.sounds.Request(ChangeSound);
    slot = NeighborChannel(services.wiiMenu.channels, slot, direction);
    services.wiiMenu.pressedChannel = slot;

    frame->StartAnimation(changeIn);
    state = StartWindowState::LeavingTheOldChannel;
}

// When ChangeIn has played, the banner of the new channel takes the place of the old one (0x813B6778).
void ChannelStartWindowScreen::RunLeavingTheOldChannel()
{
    if (frame->IsAnimationPlaying(changeIn))
        return;

    services.sounds.StopBannerSound();
    ShowTheNewChannel();
    frame->StartAnimation(changeOut);
    state = StartWindowState::ArrivingAtTheNewChannel;
}

// The new banner stands at its first frame until ChangeOut has played. Start turns gray for the Disc Channel and back (0x813B9B14).
void ChannelStartWindowScreen::ShowTheNewChannel()
{
    bool couldStart = CanStart();
    bannerLayout.reset();
    ReadTheChannel();
    BuildTheBanner();

    if (couldStart && !CanStart())
        frame->StartAnimation(grayStart);
    else if (!couldStart && CanStart())
        frame->StartAnimation(brightStart);
}

void ChannelStartWindowScreen::RunArrivingAtTheNewChannel()
{
    if (frame->IsAnimationPlaying(changeOut))
        return;

    StartTheBanner();
    state = StartWindowState::ShowingChannel;
}

void ChannelStartWindowScreen::ReactToButton(const ButtonEvent& event)
{
    bool isWiiMenu = event.buttonIndex == WiiMenuButton;
    switch (event.kind)
    {
    case ButtonEventKind::FocusGained:
        frame->StartAnimation(isWiiMenu ? focusOnOfWiiMenu : focusOnOfStart);
        services.sounds.Request(FocusSound);
        break;

    case ButtonEventKind::FocusLost:
        frame->StartAnimation(isWiiMenu ? focusOffOfWiiMenu : focusOffOfStart);
        break;

    case ButtonEventKind::Pressed:
        if (isWiiMenu)
            CloseTowardsTheGrid();
        else
            PressStart();
        break;
    }
}

// What Start does (0x813B9488 and 0x813B6F10). WeeOS has no system update and no parental controls, so the
// original's Update button of an old channel and its PIN dialog are not there. The original asks a channel that needs the Internet whether
// to set the Internet up first while no connection has passed its test; the PC's connection counts as set up, so that question never
// comes either.
void ChannelStartWindowScreen::PressStart()
{
    if (!CanStart())
    {
        services.sounds.Request(GraySound);
        return;
    }

    frame->StartAnimation(pressOfStart);
    services.sounds.Request(DecideSound);

    if (!services.titleLaunch.HasProgram(titleId))
    {
        ShowMessage(CannotBeUsedMessage);
        return;
    }

    BeginStart();
}

// WeeOS shows the menu's own message for a channel that it has no program for yet, in a dialog instead of the error screen
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

void ChannelStartWindowScreen::ReadAnswerOfCannotBeUsed()
{
    if (services.dialogs.TakeAnswer())
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
    services.sounds.StopAllSounds(LeavingStopFadeSteps);
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

// Wii Menu: the button flashes, the grid zooms out and the window fades away inside the television (0x813BAC50, 0x813B76D8).
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

ChannelGridScreen* ChannelStartWindowScreen::Grid()
{
    return dynamic_cast<ChannelGridScreen*>(services.screens.FindScreen(ScreenId::ChannelGrid));
}

BottomBarScreen* ChannelStartWindowScreen::Bar()
{
    return dynamic_cast<BottomBarScreen*>(services.screens.FindScreen(ScreenId::BottomBar));
}

bool ChannelStartWindowScreen::IsTheGridZooming()
{
    ChannelGridScreen* grid = Grid();
    return grid != nullptr && grid->Zoom() != nullptr && grid->Zoom()->IsMoving();
}

// While the grid zooms, the window is a picture inside the television, with black around it (0x813B5A54, pass 0 and pass 1).
void ChannelStartWindowScreen::Draw(DrawPass pass)
{
    if (state == StartWindowState::WaitingForTheZoom)
        return;

    if (state == StartWindowState::ZoomingIn || state == StartWindowState::ZoomingOut)
    {
        DrawWhileTheGridZooms(pass);
        return;
    }

    if (pass == DrawPass::Main)
        DrawTheWindow();
}

void ChannelStartWindowScreen::DrawTheWindow()
{
    bannerLayout->Draw();
    frame->Draw();
}

void ChannelStartWindowScreen::DrawWhileTheGridZooms(DrawPass pass)
{
    ChannelGridScreen* grid = Grid();
    if (grid == nullptr || grid->Zoom() == nullptr)
        return;

    if (pass == DrawPass::Underlay)
        zoomedWindow.DrawPicture([this] { DrawTheWindow(); });

    if (pass == DrawPass::Main)
        zoomedWindow.ShowInsideTheTelevision(*grid->Zoom());
}

}
