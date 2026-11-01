#include "SystemMenu/SdCardMenu/SdCardMenuScreens.h"

#include "SystemMenu/BackgroundServices/NandFileAccess.h"
#include "SystemMenu/BackgroundServices/SharedFlagsFile.h"
#include "SystemMenu/DataManagement/ChannelBackup.h"
#include "SystemMenu/DataManagement/DataActions.h"
#include "SystemMenu/CursorManager.h"
#include "SystemMenu/DialogManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SdCardMenu/SdChannelPlaces.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/WiiMenu/LayoutParts.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"

#include "Formats/BannerContent.h"
#include "Formats/FormatError.h"
#include "Input/RemoteButtons.h"
#include "Layout/LayoutError.h"
#include "Text/MessageCatalog.h"

#include <array>
#include <cmath>
#include <format>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::uint16_t SdCardMenuMessage = 0xA0;
constexpr std::uint16_t OkMessage = 0x2E;
constexpr std::uint16_t NoCardMessage = 0xA9;
constexpr std::uint16_t UnusableCardMessage = 0xAB;
constexpr std::uint16_t TooManyChannelsMessage = 0xC1;
constexpr std::uint16_t WriteProtectedMessage = 0xC6;

constexpr std::string_view PageSound = "WSD_SELECT";
constexpr std::string_view ChannelTargetedSound = "WIPL_SE_CH_TARGETTING";
constexpr std::string_view PushSound = "WIPL_SE_BT_PUSH";
constexpr std::string_view HoldSound = "WIPL_SE_CH_HOLD";
constexpr std::string_view SetSound = "WIPL_SE_CH_SET";
constexpr std::string_view NotMovedSound = "WIPL_SE_CH_NOT_MOVE";

// A channel is pressed with A; B held down while A goes down picks it up, as in the channel grid (0x813E28B4, SYS-82).
constexpr ButtonSet PickUpHoldButtons = {Input::RemoteButtons::B, Input::ClassicButtons::B};

// A held channel that rests on an arrow of the bar for more than 14 steps turns the page, and again after as many (0x813E168C).
constexpr int StepsOnAnArrowBeforeThePageTurns = 15;

// The buttons of the bar, in the order of the button group.
constexpr std::size_t WiiMenuButton = 0;
constexpr std::size_t HelpButton = 1;
constexpr std::size_t NextPageButton = 2;
constexpr std::size_t PreviousPageButton = 3;

// The Wii Menu button and the help button have balloons, the arrows none (0x813E95E0, the messages of the table 0x81694B88). A balloon
// appears 17 steps after the hand came, 50 units above the middle of its button, and keeps 120 units from the edges of the 16:9
// picture (0x813EA55C), as the balloons of the Wii Menu's bar.
constexpr std::uint16_t WiiMenuBalloonMessage = 168;
constexpr std::uint16_t HelpBalloonMessage = 166;
constexpr std::string_view BalloonBundleName = "balloon";
constexpr int StepsBeforeABarBalloonAppears = 17;
constexpr float BalloonHeightAboveTheButton = 50;
constexpr float BalloonMarginFromTheEdges = 120;

constexpr ButtonSet NextPageButtons = {Input::RemoteButtons::Plus, Input::ClassicButtons::Plus};
constexpr ButtonSet PreviousPageButtons = {Input::RemoteButtons::Minus, Input::ClassicButtons::Minus};

// While the window of a channel covers the picture, the list paints it black.
constexpr Formats::Rgba8Color Black = {0, 0, 0, 255};
constexpr std::uint8_t FullyShown = 255;

// A tile of the list is as big as the icon of a channel.
constexpr float TileWidth = 128;
constexpr float TileHeight = 96;

// A tile names its channel in a balloon below it, as the televisions of the Wii Menu do (0x813E3F74, 0x813E4558).
constexpr int StepsBeforeABalloonAppears = 20;
constexpr std::string_view BalloonSound = "WIPL_SE_BALLOON";

constexpr std::uint32_t EmptyPlace = 0;

std::string TilePaneName(std::size_t placeOfPage)
{
    return std::format("N_Ch_c{:02}", placeOfPage + 1);
}

}

// The pages that explain the SD Card Menu: the first time it opens (0x8165439C) and behind its help button (0x8165440C).
enum class PictureOfAPage
{
    None,
    WaitIcon,
    HelpButton,
};

struct ExplanationPage
{
    std::uint16_t message;
    std::uint16_t nextButtonMessage;
    bool hasBackButton;
    PictureOfAPage picture;
    float pictureHeight;
};

constexpr std::uint16_t NextMessage = 0xA3;
constexpr std::uint16_t CloseMessage = 0xA4;
constexpr std::uint16_t BackMessage = 0xA5;

// How far above the middle of the dialog the pictures of the pages lie.
constexpr float WaitIconHeight = 74;
constexpr float HelpButtonHeight = 108;

// "Welcome to the SD Card Menu!", "About Save Data", "Caution!" with the turning icon, the help button.
constexpr std::array<ExplanationPage, 4> IntroPages = {{
    {0x9D, NextMessage, false, PictureOfAPage::None, 0},
    {0x9E, NextMessage, true, PictureOfAPage::None, 0},
    {0x9F, NextMessage, true, PictureOfAPage::WaitIcon, WaitIconHeight},
    {0xCA, CloseMessage, true, PictureOfAPage::HelpButton, HelpButtonHeight},
}};

// "About the SD Card Menu", "About Save Data", "Caution!" with the turning icon.
constexpr std::array<ExplanationPage, 3> HelpPages = {{
    {0xC9, NextMessage, false, PictureOfAPage::None, 0},
    {0x9E, NextMessage, true, PictureOfAPage::None, 0},
    {0x9F, CloseMessage, true, PictureOfAPage::WaitIcon, WaitIconHeight},
}};

namespace
{

SdChannelListScreen* FindList(ScreenManager& screens)
{
    return static_cast<SdChannelListScreen*>(screens.FindScreen(ScreenId::SdChannelList));
}

bool WentDown(const PlayerInput& player, const ButtonSet& buttons)
{
    bool remoteWentDown = (player.state.buttonsPressed & buttons.remoteButtons) != 0;
    bool classicWentDown = (player.state.classic.buttonsPressed & buttons.classicButtons) != 0;
    return remoteWentDown || classicWentDown;
}

bool IsHeld(const PlayerInput& player, const ButtonSet& buttons)
{
    bool remoteHeld = (player.state.buttonsHeld & buttons.remoteButtons) != 0;
    bool classicHeld = (player.state.classic.buttonsHeld & buttons.classicButtons) != 0;
    return remoteHeld || classicHeld;
}

}

SdCardMenuBarScreen::SdCardMenuBarScreen(MenuServices& menuServices)
    : Screen(ScreenId::SdCardMenuBar)
    , services(menuServices)
{
}

void SdCardMenuBarScreen::Load()
{
    archive = services.files.OpenLayoutBundle("sdButton");
    balloonArchive = services.files.OpenLayoutBundle(BalloonBundleName);
}

void SdCardMenuBarScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, "mn_SdcardMenu_b");
    layout->SetText("T_page", services.messages.Find(SdCardMenuMessage));
    layout->Calculate();
    std::vector<std::string> buttonPanes = {"B_Wiimenu", "B_Help", "B_ArwR", "B_ArwL"};
    buttons = std::make_unique<ButtonController>(*layout, services.display, buttonPanes);

    wiiMenuBalloon = std::make_unique<Balloon>(services.display, *balloonArchive, StepsBeforeABarBalloonAppears);
    wiiMenuBalloon->SetText(services.messages.Find(WiiMenuBalloonMessage));
    helpBalloon = std::make_unique<Balloon>(services.display, *balloonArchive, StepsBeforeABarBalloonAppears);
    helpBalloon->SetText(services.messages.Find(HelpBalloonMessage));

    services.screens.OpenChild(ScreenId::SdChannelList, Id(), 0);
    services.fader.StartFadeIn();
}

// The bar is the background of the menu and is drawn below every screen. Its arrows are drawn once more over the list, which is what
// the screen 0x25 of the original does.
void SdCardMenuBarScreen::Draw(DrawPass pass)
{
    if (pass == DrawPass::Underlay)
        layout->Draw();

    bool isWindowOpen = services.screens.FindScreen(ScreenId::SdChannelWindow) != nullptr;
    if (pass != DrawPass::Overlay || isWindowOpen)
        return;

    DrawArrowsOnly();
    wiiMenuBalloon->Draw();
    helpBalloon->Draw();
}

void SdCardMenuBarScreen::DrawArrowsOnly()
{
    std::vector<Formats::Pane*> hiddenPanes = {&layout->PaneNamed("N_BtnR"), &layout->PaneNamed("N_BtnL")};
    for (Formats::Pane& pane : layout->PaneNamed("RootPane").children)
    {
        if (pane.name != "N_Top")
            hiddenPanes.push_back(&pane);
    }

    std::vector<bool> wasVisible;
    for (Formats::Pane* pane : hiddenPanes)
    {
        wasVisible.push_back(pane->isVisible);
        pane->isVisible = false;
    }

    layout->Draw();

    for (std::size_t index = 0; index < hiddenPanes.size(); ++index)
        hiddenPanes[index]->isVisible = wasVisible[index];
}

const Balloon& SdCardMenuBarScreen::WiiMenuBalloon() const
{
    return *wiiMenuBalloon;
}

const Balloon& SdCardMenuBarScreen::HelpBalloon() const
{
    return *helpBalloon;
}

int SdCardMenuBarScreen::PageArrowAt(Formats::Vector2 place)
{
    if (layout->IsPointInPane("B_ArwR", place))
        return 1;

    if (layout->IsPointInPane("B_ArwL", place))
        return -1;

    return 0;
}

bool SdCardMenuBarScreen::UpdatesWithChildren() const
{
    return true;
}

bool SdCardMenuBarScreen::DrawsWithChildren() const
{
    return true;
}

// The bar answers its buttons while no dialog is open (0x813E2B54 handles them for the list).
bool SdCardMenuBarScreen::RunMain()
{
    layout->Calculate();
    CalculateTheBalloons();

    // The window of a channel lies over the bar and answers its own dialogs. A pressed or held channel takes the balloons away
    // (0x813EA75C).
    if (services.screens.FindScreen(ScreenId::SdChannelWindow) != nullptr)
    {
        wiiMenuBalloon->PointerLeft();
        helpBalloon->PointerLeft();
        return false;
    }

    bool isActive = !services.dialogs.IsOpen();

    // The help and the notice of too many channels need no answer.
    services.dialogs.TakeAnswer();

    for (const ButtonEvent& event : buttons->Poll(services.input, isActive))
    {
        if (event.kind == ButtonEventKind::FocusGained)
            services.sounds.Request(SoundName::ButtonTargeted);

        FollowTheFocusWithTheBalloons(event);

        if (event.kind != ButtonEventKind::Pressed)
            continue;

        SdChannelListScreen* list = FindList(services.screens);
        if (event.buttonIndex == WiiMenuButton)
        {
            services.sounds.Request(SoundName::Decide);
            services.fader.StartFadeOut();
            GoToRoot(ScreenId::MainFrame, 0);
            return true;
        }

        if (event.buttonIndex == HelpButton)
        {
            if (list != nullptr)
                list->ShowHelp();
        }
        else if (list != nullptr)
        {
            list->TurnPage(event.buttonIndex == NextPageButton ? 1 : -1);
        }
    }
    return false;
}

// A balloon grows when the first hand reaches its button and shrinks when the last one leaves (0x813E9D08, 0x813E9F60).
void SdCardMenuBarScreen::FollowTheFocusWithTheBalloons(const ButtonEvent& event)
{
    Balloon* balloon = nullptr;
    if (event.buttonIndex == WiiMenuButton)
        balloon = wiiMenuBalloon.get();
    else if (event.buttonIndex == HelpButton)
        balloon = helpBalloon.get();

    if (balloon == nullptr)
        return;

    if (event.kind == ButtonEventKind::FocusGained)
        balloon->PointerArrived();
    else if (event.kind == ButtonEventKind::FocusLost)
        balloon->PointerLeft();
}

void SdCardMenuBarScreen::CalculateTheBalloons()
{
    PlacedPanes placedPanes(*layout);
    CalculateBalloon(*wiiMenuBalloon, placedPanes.PlaceOf("B_Wiimenu"));
    CalculateBalloon(*helpBalloon, placedPanes.PlaceOf("B_Help"));
}

void SdCardMenuBarScreen::CalculateBalloon(Balloon& balloon, std::optional<Formats::Vector2> placeOfTheButton)
{
    Formats::Vector2 button = placeOfTheButton.value_or(Formats::Vector2{0, 0});
    balloon.PlaceAt({button.x, button.y + BalloonHeightAboveTheButton}, BalloonMarginFromTheEdges);

    bool wasShown = balloon.IsShown();
    balloon.Calculate();
    if (!wasShown && balloon.IsShown())
        services.sounds.Request(BalloonSound);
}

bool SdCardMenuBarScreen::RunClosing()
{
    layout->Calculate();
    return services.fader.GetStatus() == FaderStatus::Covered;
}

SdChannelListScreen::SdChannelListScreen(MenuServices& menuServices)
    : Screen(ScreenId::SdChannelList)
    , services(menuServices)
{
}

void SdChannelListScreen::Load()
{
    archive = services.files.OpenLayoutBundle("sdChanSel");
}

void SdChannelListScreen::Build()
{
    gridLayout = std::make_unique<MenuLayout>(services.display, *archive, "mn_SdcardMenu_a");
    noticeLayout = std::make_unique<MenuLayout>(services.display, *archive, "mn_Nocard");
    noticeInSlot = noticeLayout->BindAnimation("mn_Nocard_IN", "Group_00", Layout::PlaybackMode::PlayOnce);
    cursorLayout = std::make_unique<MenuLayout>(services.display, *archive, "my_IplTop_d");
    waitIconLayout = std::make_unique<MenuLayout>(services.display, *archive, "wait_icon");
    waitIconLayout->BindAnimationToPane("wait_icon_wait_loop", "Wait_00", Layout::PlaybackMode::Loop);
    helpButtonLayout = std::make_unique<MenuLayout>(services.display, *archive, "help_Btn");

    // The SD Card Menu brings the flags file of the console up to date when it starts (CSFlags, 0x813682C4).
    if (services.dataManagement != nullptr)
    {
        NandFileAccess files(services.dataManagement->fileSystem, services.dataManagement->titles);
        UpdateSharedFlagsFile(files);
    }

    ReadChannels();
    gridLayout->Calculate();
}

std::size_t SdChannelListScreen::Page() const
{
    return page;
}

std::array<std::uint32_t, Formats::ChannelPlacesPerPage> SdChannelListScreen::ChannelsOfPage() const
{
    std::array<std::uint32_t, Formats::ChannelPlacesPerPage> channels = {};
    for (std::size_t placeOfPage = 0; placeOfPage < channels.size(); ++placeOfPage)
        channels[placeOfPage] = places[page * Formats::ChannelPlacesPerPage + placeOfPage];

    return channels;
}

// The channels of the card in their places. Places that changed are written back, unless the card is write protected
// (0x813DBFE0, 0x813DBE94). More channels than places are told once (0x813DCB40).
void SdChannelListScreen::ReadChannels()
{
    places = {};
    isCardReady = services.dataManagement != nullptr && ReadCardState(*services.dataManagement) == CardState::Ready;
    if (!isCardReady)
    {
        bool isCardInSlot = services.dataManagement != nullptr && services.dataManagement->sdCard.GetStatus().isInserted;
        ShowNotice(isCardInSlot ? UnusableCardMessage : NoCardMessage);
        ShowPage();
        return;
    }

    ShowNotice(std::nullopt);
    SdCardChannels channels = PlaceChannelsOfCard(*services.dataManagement);
    places = channels.places;

    bool isWriteProtected = services.dataManagement->sdCard.GetStatus().isWriteProtected;
    if (channels.mustBeWritten && !isWriteProtected)
        WriteChannelPlaces(*services.dataManagement, places);

    if (channels.hasTooManyChannels && !hasToldTooManyChannels)
    {
        hasToldTooManyChannels = true;
        DialogRequest request;
        request.layout = DialogLayout::OneButton;
        request.message = services.messages.Find(TooManyChannelsMessage);
        request.buttonBText = services.messages.Find(OkMessage);
        services.dialogs.Open(request);
    }
    ShowPage();
}

void SdChannelListScreen::ShowPage()
{
    tiles.clear();
    focusedTile.reset();
    for (std::size_t placeOfPage = 0; placeOfPage < Formats::ChannelPlacesPerPage; ++placeOfPage)
    {
        std::size_t place = page * Formats::ChannelPlacesPerPage + placeOfPage;
        std::uint32_t lowPart = places[place];
        bool isTheHeldChannel = heldChannel && heldChannel->place == place;
        if (lowPart == EmptyPlace || isTheHeldChannel)
            continue;

        Tile tile;
        tile.placeOfPage = placeOfPage;
        tile.lowPart = lowPart;
        std::optional<ChannelOnCard> channel = ReadChannelOnCard(*services.dataManagement, lowPart);
        try
        {
            if (channel)
            {
                tile.balloon = MakeBalloon(*channel);
                tile.icon = std::make_unique<ChannelIconLayout>(services.display.Device(), channel->icon);
            }
        }
        catch (const Formats::FormatError&)
        {
            tile.icon.reset();
        }
        catch (const Layout::LayoutError&)
        {
            tile.icon.reset();
        }
        tiles.push_back(std::move(tile));
    }
}

// The balloon holds the first line of the English name of the channel; a channel without a name has none.
std::unique_ptr<Balloon> SdChannelListScreen::MakeBalloon(const ChannelOnCard& channel)
{
    std::array<Formats::ChannelName, Formats::BannerLanguageCount> names =
        Formats::ReadChannelNames(channel.header.bannerHeader);
    std::u16string name = names[Formats::EnglishLanguageNumber].firstLine;
    if (name.empty())
        return nullptr;

    auto balloon = std::make_unique<Balloon>(services.display, *archive, StepsBeforeABalloonAppears);
    balloon->SetText(name);
    return balloon;
}

void SdChannelListScreen::ShowNotice(std::optional<std::uint16_t> messageNumber)
{
    isNoticeShown = messageNumber.has_value();
    if (!messageNumber)
        return;

    noticeLayout->SetText("T_TimerMes", services.messages.Find(*messageNumber));
    noticeLayout->StartAnimation(noticeInSlot);
}

void SdChannelListScreen::TurnPage(int direction)
{
    bool canGoForward = direction > 0 && page + 1 < Formats::ChannelPlacePageCount;
    bool canGoBack = direction < 0 && page > 0;
    if (!canGoForward && !canGoBack)
        return;

    services.sounds.Request(PageSound);
    page = canGoForward ? page + 1 : page - 1;
    ShowPage();
}

// The pages are shown in the dialog window with two buttons, the words of one page after the other (0x81346C14).
void SdChannelListScreen::OpenExplanation(std::span<const ExplanationPage> explanationPages)
{
    DialogRequest request;
    for (const ExplanationPage& explanationPage : explanationPages)
    {
        DialogPage dialogPage;
        dialogPage.message = services.messages.Find(explanationPage.message);
        dialogPage.nextText = services.messages.Find(explanationPage.nextButtonMessage);
        dialogPage.backText = services.messages.Find(BackMessage);
        dialogPage.hasBackButton = explanationPage.hasBackButton;
        dialogPage.picture = PictureLayout(explanationPage.picture);
        dialogPage.pictureHeight = explanationPage.pictureHeight;
        dialogPage.playsPictureAnimation = explanationPage.picture == PictureOfAPage::WaitIcon;
        request.pages.push_back(dialogPage);
    }
    services.dialogs.Open(request);
}

MenuLayout* SdChannelListScreen::PictureLayout(PictureOfAPage picture)
{
    if (picture == PictureOfAPage::WaitIcon)
        return waitIconLayout.get();

    if (picture == PictureOfAPage::HelpButton)
        return helpButtonLayout.get();

    return nullptr;
}

// The help button of the bar opens three pages about the menu (0x813E2B54, 0x813DDD64).
void SdChannelListScreen::ShowHelp()
{
    if (services.dialogs.IsOpen())
        return;

    services.sounds.Request(SoundName::Decide);
    OpenExplanation(HelpPages);
}

// The first time the menu opens, it explains itself on four pages once the picture has faded in, and the save file of the menu
// remembers that it has (0x813DDE90, 0x813DDC80, 0x813DDCD8).
void SdChannelListScreen::ExplainTheMenuTheFirstTime()
{
    if (isShowingIntro)
    {
        if (services.dialogs.IsOpen())
            return;

        isShowingIntro = false;
        services.wiiMenu.channels.MarkSdCardMenuIntroSeen();
        return;
    }

    if (hasCheckedIntro || services.fader.GetStatus() != FaderStatus::Clear)
        return;

    hasCheckedIntro = true;
    if (!services.wiiMenu.channels.IsSdCardMenuIntroPending() || services.dialogs.IsOpen())
        return;

    OpenExplanation(IntroPages);
    isShowingIntro = true;
}

bool SdChannelListScreen::RunMain()
{
    if (HasChildren())
    {
        RunZoom();
        return false;
    }

    ExplainTheMenuTheFirstTime();

    bool isCardReadyNow = services.dataManagement != nullptr && ReadCardState(*services.dataManagement) == CardState::Ready;
    if (isCardReadyNow != isCardReady)
        ReadChannels();

    if (services.input.AnyPlayerPressed(NextPageButtons))
        TurnPage(1);
    else if (services.input.AnyPlayerPressed(PreviousPageButtons))
        TurnPage(-1);

    if (!services.dialogs.IsOpen())
        FollowThePointer();

    gridLayout->Calculate();
    noticeLayout->Calculate();
    cursorLayout->Calculate();
    PlaceTiles();
    return false;
}

// The hand on a tile puts the cursor around it; A on it asks for the window of the channel, and B held with A picks the channel up
// (0x813E28B4, 0x813E0450, 0x813E22F8).
void SdChannelListScreen::FollowThePointer()
{
    if (heldChannel)
    {
        RunHolding();
        return;
    }

    std::optional<std::size_t> tileUnderPointer = TileUnderPointer();
    if (tileUnderPointer != focusedTile && tileUnderPointer)
        services.sounds.Request(ChannelTargetedSound);

    focusedTile = tileUnderPointer;
    FollowThePointerWithTheBalloons();
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
        std::optional<std::size_t> tile = TileUnderPointerOf(playerIndex);
        if (!player || !tile || !WentDown(*player, SelectButtons))
            continue;

        if (IsHeld(*player, PickUpHoldButtons))
            PickUpChannel(playerIndex, *tile);
        else
            PressChannel(*tile);
        return;
    }
}

void SdChannelListScreen::FollowThePointerWithTheBalloons()
{
    for (std::size_t tileIndex = 0; tileIndex < tiles.size(); ++tileIndex)
    {
        Balloon* balloon = tiles[tileIndex].balloon.get();
        if (balloon == nullptr)
            continue;

        if (focusedTile == tileIndex)
            balloon->PointerArrived();
        else
            balloon->PointerLeft();
    }
}

void SdChannelListScreen::PressChannel(std::size_t tileIndex)
{
    for (Tile& tile : tiles)
    {
        if (tile.balloon)
            tile.balloon->PointerLeft();
    }

    services.sounds.Request(PushSound);
    pressedTile = tileIndex;
    std::size_t place = page * Formats::ChannelPlacesPerPage + tiles[tileIndex].placeOfPage;
    services.screens.OpenChild(ScreenId::SdChannelWindow, Id(), static_cast<int>(place));
}

// The channel leaves its place and follows the hand, which closes around it (0x813E22F8).
void SdChannelListScreen::PickUpChannel(int playerIndex, std::size_t tileIndex)
{
    // A card that is write-protected keeps its order, and says so once (0x813E28B4).
    if (services.dataManagement->sdCard.GetStatus().isWriteProtected)
    {
        if (hasToldWriteProtection)
            return;

        hasToldWriteProtection = true;
        DialogRequest request;
        request.layout = DialogLayout::OneButton;
        request.message = services.messages.Find(WriteProtectedMessage);
        request.buttonBText = services.messages.Find(OkMessage);
        services.dialogs.Open(request);
        return;
    }

    Tile& tile = tiles[tileIndex];
    HeldChannel held;
    held.playerIndex = playerIndex;
    held.place = page * Formats::ChannelPlacesPerPage + tile.placeOfPage;
    held.icon = std::move(tile.icon);
    heldChannel = std::move(held);

    tiles.erase(tiles.begin() + static_cast<std::ptrdiff_t>(tileIndex));
    focusedTile.reset();
    stepsOnAnArrow = 0;
    services.cursors.SetHandGrabbing(playerIndex, true);
    services.sounds.Request(HoldSound);
}

// While A stays down the channel follows the hand; resting on an arrow of the bar turns the page under it (0x813E1568, 0x813E168C).
void SdChannelListScreen::RunHolding()
{
    const std::optional<PlayerInput>& player = services.input.Player(heldChannel->playerIndex);
    if (!player || !IsHeld(*player, SelectButtons))
    {
        DropChannel();
        return;
    }

    std::optional<Formats::Vector2> place = PointerPlaceOf(heldChannel->playerIndex);
    auto* bar = dynamic_cast<SdCardMenuBarScreen*>(services.screens.FindScreen(ScreenId::SdCardMenuBar));
    int arrow = 0;
    if (place && bar)
        arrow = bar->PageArrowAt(*place);
    if (arrow != arrowUnderTheHand)
        stepsOnAnArrow = 0;

    arrowUnderTheHand = arrow;
    if (arrow == 0)
        return;

    ++stepsOnAnArrow;
    if (stepsOnAnArrow < StepsOnAnArrowBeforeThePageTurns)
        return;

    stepsOnAnArrow = 0;
    TurnPage(arrow);
}

// The channel lands on an empty place under the hand, or on its own place; anywhere else it goes back (0x813E24D8, 0x813E26DC).
// The new places are written to loc.dat at once.
void SdChannelListScreen::DropChannel()
{
    HeldChannel held = std::move(*heldChannel);
    heldChannel.reset();
    services.cursors.SetHandGrabbing(held.playerIndex, false);

    std::optional<Formats::Vector2> pointerPlace = PointerPlaceOf(held.playerIndex);
    std::optional<std::size_t> placeOfPage = pointerPlace ? PlaceOfPageAt(*pointerPlace) : std::nullopt;
    std::optional<std::size_t> target;
    if (placeOfPage)
        target = page * Formats::ChannelPlacesPerPage + *placeOfPage;

    bool canLand = target && (*target == held.place || places[*target] == EmptyPlace);
    services.sounds.Request(canLand ? SetSound : NotMovedSound);
    if (canLand && *target != held.place)
    {
        MoveChannelToPlace(places, held.place, *target);
        bool isWriteProtected = services.dataManagement->sdCard.GetStatus().isWriteProtected;
        if (!isWriteProtected)
            WriteChannelPlaces(*services.dataManagement, places);
    }
    ShowPage();
}

bool SdChannelListScreen::IsHoldingAChannel() const
{
    return heldChannel.has_value();
}

std::uint32_t SdChannelListScreen::PressedChannel() const
{
    return pressedTile ? tiles[*pressedTile].lowPart : 0;
}

void SdChannelListScreen::ZoomIntoPressedChannel()
{
    if (!pressedTile)
        return;

    Formats::Vector2 stretch = services.display.LayoutStretch();
    std::string tilePane = TilePaneName(tiles[*pressedTile].placeOfPage);
    std::optional<Formats::Vector2> middle = gridLayout->PointOfPaneInPicture(tilePane, {0, 0});
    Formats::Vector2 place = middle.value_or(Formats::Vector2{0, 0});
    float halfWidth = TileWidth * stretch.x / 2;
    float halfHeight = TileHeight / 2;
    PictureArea television = {place.x - halfWidth, place.x + halfWidth, place.y - halfHeight, place.y + halfHeight};
    zoom = std::make_unique<TelevisionZoom>(services.display.WholePicture(), television);
    zoom->StartZoomingIn();
}

void SdChannelListScreen::ZoomOutOfPressedChannel()
{
    if (!zoom)
        return;

    zoom->StartZoomingOut();
    isCoveredByWindow = false;
}

const TelevisionZoom* SdChannelListScreen::Zoom() const
{
    return zoom.get();
}

bool SdChannelListScreen::UpdatesWithChildren() const
{
    return true;
}

bool SdChannelListScreen::DrawsWithChildren() const
{
    return true;
}

// When the zoom has come in, the window covers the picture; when it has gone out again, the list is as it was.
void SdChannelListScreen::RunZoom()
{
    if (!zoom)
        return;

    zoom->Step();
    if (zoom->IsMoving())
        return;

    if (zoom->Closeness() > 0)
    {
        isCoveredByWindow = true;
        return;
    }

    zoom.reset();
}

std::optional<std::size_t> SdChannelListScreen::TileUnderPointer() const
{
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        std::optional<std::size_t> tile = TileUnderPointerOf(playerIndex);
        if (tile)
            return tile;
    }
    return std::nullopt;
}

std::optional<std::size_t> SdChannelListScreen::TileUnderPointerOf(int playerIndex) const
{
    std::optional<Formats::Vector2> pointer = PointerPlaceOf(playerIndex);
    std::optional<std::size_t> placeOfPage = pointer ? PlaceOfPageAt(*pointer) : std::nullopt;
    if (!placeOfPage)
        return std::nullopt;

    for (std::size_t tileIndex = 0; tileIndex < tiles.size(); ++tileIndex)
    {
        if (tiles[tileIndex].placeOfPage == *placeOfPage)
            return tileIndex;
    }
    return std::nullopt;
}

// The place of the page, filled or empty, whose television the point lies on.
std::optional<std::size_t> SdChannelListScreen::PlaceOfPageAt(Formats::Vector2 point) const
{
    Formats::Vector2 stretch = services.display.LayoutStretch();
    for (std::size_t placeOfPage = 0; placeOfPage < Formats::ChannelPlacesPerPage; ++placeOfPage)
    {
        std::optional<Formats::Vector2> middle = gridLayout->PointOfPaneInPicture(TilePaneName(placeOfPage), {0, 0});
        if (!middle)
            continue;

        bool isInsideSideways = std::abs(point.x - middle->x) <= TileWidth * stretch.x / 2;
        bool isInsideUpAndDown = std::abs(point.y - middle->y) <= TileHeight / 2;
        if (isInsideSideways && isInsideUpAndDown)
            return placeOfPage;
    }
    return std::nullopt;
}

std::optional<Formats::Vector2> SdChannelListScreen::PointerPlaceOf(int playerIndex) const
{
    const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
    if (!player || !player->state.pointerIsValid)
        return std::nullopt;

    return services.display.PointerToCanvas(player->state.pointer);
}

void SdChannelListScreen::PlaceTiles()
{
    Formats::Vector2 stretch = services.display.LayoutStretch();
    for (Tile& tile : tiles)
    {
        std::optional<Formats::Vector2> middle = gridLayout->PointOfPaneInPicture(TilePaneName(tile.placeOfPage), {0, 0});
        if (!middle || !tile.icon)
            continue;

        tile.icon->Place(*middle, stretch, {TileWidth, TileHeight});
        tile.icon->Calculate();
        if (!tile.balloon)
            continue;

        tile.balloon->PlaceBelow(*middle, TileHeight / 2);
        bool wasShown = tile.balloon->IsShown();
        tile.balloon->Calculate();
        if (!wasShown && tile.balloon->IsShown())
            services.sounds.Request(BalloonSound);
    }

    PlaceHeldChannel();
    PlaceCursor();
}

void SdChannelListScreen::PlaceHeldChannel()
{
    if (!heldChannel || !heldChannel->icon)
        return;

    std::optional<Formats::Vector2> handPlace = PointerPlaceOf(heldChannel->playerIndex);
    if (!handPlace)
        return;

    heldChannel->icon->Place(*handPlace, services.display.LayoutStretch(), {TileWidth, TileHeight});
    heldChannel->icon->Calculate();
}

void SdChannelListScreen::PlaceCursor()
{
    Formats::Pane& cursor = cursorLayout->PaneNamed("Cursur_a");
    cursor.isVisible = focusedTile.has_value();
    if (!focusedTile)
        return;

    std::string tilePane = TilePaneName(tiles[*focusedTile].placeOfPage);
    std::optional<Formats::Vector2> middle = gridLayout->PointOfPaneInPicture(tilePane, {0, 0});
    if (middle)
    {
        Formats::Vector2 placeBelowRoot = services.display.CanvasToRootPaneSpace(*middle);
        cursor.translation = {placeBelowRoot.x, placeBelowRoot.y, 0};
    }
}

void SdChannelListScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main)
        return;

    if (isCoveredByWindow)
    {
        services.display.FillArea(services.display.WholePicture(), Black);
        return;
    }

    if (zoom)
        services.display.MoveArea(zoom->VisibleArea(), services.display.WholePicture(), FullyShown);

    DrawTheList();
    services.display.StopMovingAreas();
}

void SdChannelListScreen::DrawTheList()
{
    gridLayout->Draw();
    for (Tile& tile : tiles)
    {
        if (tile.icon)
            tile.icon->Draw();
    }
    cursorLayout->Draw();

    if (heldChannel && heldChannel->icon)
        heldChannel->icon->Draw();

    for (Tile& tile : tiles)
    {
        if (tile.balloon)
            tile.balloon->Draw();
    }

    if (isNoticeShown)
        noticeLayout->Draw();
}

}
