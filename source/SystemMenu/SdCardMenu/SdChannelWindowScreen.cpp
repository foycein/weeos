#include "SystemMenu/SdCardMenu/SdChannelWindowScreen.h"

#include "SystemMenu/DataManagement/ChannelBackup.h"
#include "SystemMenu/DialogManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SdCardMenu/SdCardMenuScreens.h"
#include "SystemMenu/SoundRequests.h"

#include "Formats/FormatError.h"
#include "Text/MessageCatalog.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view WindowBundle = "sdChanTtl";
constexpr std::string_view FrameLayout = "mn_SdcardMenuBanner_bc";
constexpr std::string_view BackgroundLayout = "mn_SdcardMenuBanner_a";

constexpr std::size_t WiiMenuButton = 0;

// The animations of the buttons, as in the channel start window, under the name of this layout.
constexpr std::string_view FocusOnAnimation = "mn_SdcardMenuBanner_bc_FocusBtn_on";
constexpr std::string_view FocusOffAnimation = "mn_SdcardMenuBanner_bc_FocusBtnA_off";
constexpr std::string_view PressAnimation = "mn_SdcardMenuBanner_bc_SelectBtn_Ac";

constexpr std::uint16_t WiiMenuMessage = 1;
constexpr std::uint16_t StartMessage = 2;
constexpr std::uint16_t CannotBeUsedMessage = 19;
constexpr std::uint16_t OkMessage = 46;

// The icon of the channel fills the picture pane Ch_Picture, 128 by 96 units.
constexpr std::string_view IconPane = "Ch_Picture";
constexpr Formats::Vector2 IconBox = {128, 96};

constexpr std::string_view WindowSound = "WIPL_ME_SD_BANNER";
constexpr std::string_view FocusSound = "WIPL_SE_BT_TARGETTING";
constexpr std::string_view PushSound = "WIPL_SE_BT_PUSH";
constexpr std::string_view DecideSound = "WIPL_SE_DECIDE";
constexpr std::string_view CloseSound = "WIPL_SE_CH_UNSELECT";

}

SdChannelWindowScreen::SdChannelWindowScreen(MenuServices& menuServices)
    : Screen(ScreenId::SdChannelWindow)
    , services(menuServices)
    , zoomedWindow(menuServices.display)
{
}

SdWindowState SdChannelWindowScreen::State() const
{
    return state;
}

// The name and the icon come from the header of content.bin (Data Management reads them for the list too).
void SdChannelWindowScreen::Load()
{
    archive = services.files.OpenLayoutBundle(WindowBundle);
    SdChannelListScreen* list = List();
    if (list == nullptr || services.dataManagement == nullptr)
        return;

    std::optional<ChannelOnCard> channel = ReadChannelOnCard(*services.dataManagement, list->PressedChannel());
    if (!channel)
        return;

    try
    {
        name = Formats::ReadChannelNames(channel->header.bannerHeader)[Formats::EnglishLanguageNumber].firstLine;
    }
    catch (const Formats::FormatError&)
    {
        name.clear();
    }
    iconFile = channel->icon;
}

void SdChannelWindowScreen::Build()
{
    frame = std::make_unique<MenuLayout>(services.display, *archive, std::string(FrameLayout));
    frame->SetText("T_BtnA", services.messages.Find(WiiMenuMessage));
    frame->SetText("T_BtnB", services.messages.Find(StartMessage));
    focusOffOfWiiMenu = frame->BindAnimation(std::string(FocusOffAnimation), "G_FocusBtnA", Layout::PlaybackMode::PlayOnce);
    focusOnOfWiiMenu = frame->BindAnimation(std::string(FocusOnAnimation), "G_FocusBtnA", Layout::PlaybackMode::PlayOnce);
    focusOffOfStart = frame->BindAnimation(std::string(FocusOffAnimation), "G_FocusBtnB", Layout::PlaybackMode::PlayOnce);
    focusOnOfStart = frame->BindAnimation(std::string(FocusOnAnimation), "G_FocusBtnB", Layout::PlaybackMode::PlayOnce);
    pressOfWiiMenu = frame->BindAnimation(std::string(PressAnimation), "G_SelectBtnA", Layout::PlaybackMode::PlayOnce);
    pressOfStart = frame->BindAnimation(std::string(PressAnimation), "G_SelectBtnB", Layout::PlaybackMode::PlayOnce);
    buttons = std::make_unique<ButtonController>(*frame, services.display, std::vector<std::string>{"B_BtnA", "B_BtnB"});

    background = std::make_unique<MenuLayout>(services.display, *archive, std::string(BackgroundLayout));
    background->SetText("T_title", name);

    try
    {
        if (!iconFile.empty())
            icon = std::make_unique<ChannelIconLayout>(services.display.Device(), iconFile);
    }
    catch (const std::runtime_error&)
    {
        icon.reset();
    }
}

// The icon stands where the layout has the picture of the channel. Its reflection below (Chm_Picture) stays as the layout has it.
void SdChannelWindowScreen::BeforeStep()
{
    frame->Calculate();
    background->Calculate();
    std::optional<Formats::Vector2> middle = background->PointOfPaneInPicture(IconPane, {0, 0});
    if (!icon || !middle)
        return;

    icon->Place(*middle, services.display.LayoutStretch(), IconBox);
    icon->Calculate();
}

bool SdChannelWindowScreen::IsReady()
{
    SdChannelListScreen* list = List();
    if (state == SdWindowState::WaitingForTheZoom)
    {
        if (list != nullptr)
            list->ZoomIntoPressedChannel();

        state = SdWindowState::ZoomingIn;
        return false;
    }

    bool isZooming = list != nullptr && list->Zoom() != nullptr && list->Zoom()->IsMoving();
    return !isZooming;
}

void SdChannelWindowScreen::OnStart()
{
    services.sounds.Request(WindowSound);
    state = SdWindowState::ShowingChannel;
}

bool SdChannelWindowScreen::RunMain()
{
    if (state == SdWindowState::ZoomingOut)
        return List() == nullptr || List()->Zoom() == nullptr;

    if (state == SdWindowState::ShowingCannotBeUsed)
    {
        if (services.dialogs.TakeAnswer())
            state = SdWindowState::ShowingChannel;
        return false;
    }

    bool isActive = services.fader.GetStatus() == FaderStatus::Clear;
    for (const ButtonEvent& event : buttons->Poll(services.input, isActive))
        ReactToButton(event);

    return false;
}

void SdChannelWindowScreen::ReactToButton(const ButtonEvent& event)
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
            CloseTowardsTheList();
        else
            PressStart();
        break;
    }
}

void SdChannelWindowScreen::PressStart()
{
    frame->StartAnimation(pressOfStart);
    services.sounds.Request(DecideSound);

    DialogRequest request;
    request.layout = DialogLayout::OneButton;
    request.message = services.messages.Find(CannotBeUsedMessage);
    request.buttonBText = services.messages.Find(OkMessage);
    services.dialogs.Open(request);
    state = SdWindowState::ShowingCannotBeUsed;
}

void SdChannelWindowScreen::CloseTowardsTheList()
{
    frame->StartAnimation(pressOfWiiMenu);
    services.sounds.Request(PushSound);
    services.sounds.Request(CloseSound);
    SdChannelListScreen* list = List();
    if (list != nullptr)
        list->ZoomOutOfPressedChannel();

    state = SdWindowState::ZoomingOut;
}

SdChannelListScreen* SdChannelWindowScreen::List()
{
    return dynamic_cast<SdChannelListScreen*>(services.screens.FindScreen(ScreenId::SdChannelList));
}

// While the list zooms, the window is a picture inside the television (0x813E5F54, pass 0 and pass 1).
void SdChannelWindowScreen::Draw(DrawPass pass)
{
    if (state == SdWindowState::WaitingForTheZoom)
        return;

    bool isZooming = state == SdWindowState::ZoomingIn || state == SdWindowState::ZoomingOut;
    SdChannelListScreen* list = List();
    if (isZooming && list != nullptr && list->Zoom() != nullptr)
    {
        if (pass == DrawPass::Underlay)
            zoomedWindow.DrawPicture([this] { DrawTheWindow(); });

        if (pass == DrawPass::Main)
            zoomedWindow.ShowInsideTheTelevision(*list->Zoom());
        return;
    }

    if (pass == DrawPass::Main && !isZooming)
        DrawTheWindow();
}

void SdChannelWindowScreen::DrawTheWindow()
{
    background->Draw();
    if (icon)
        icon->Draw();

    frame->Draw();
}

}
