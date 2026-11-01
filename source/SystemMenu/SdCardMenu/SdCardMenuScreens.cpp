#include "SystemMenu/SdCardMenu/SdCardMenuScreens.h"

#include "SystemMenu/BackgroundServices/NandFileAccess.h"
#include "SystemMenu/BackgroundServices/SharedFlagsFile.h"
#include "SystemMenu/DataManagement/ChannelBackup.h"
#include "SystemMenu/DataManagement/DataActions.h"
#include "SystemMenu/DialogManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SdCardMenu/SdChannelPlaces.h"
#include "SystemMenu/SoundRequests.h"

#include "Formats/FormatError.h"
#include "Input/RemoteButtons.h"
#include "Layout/LayoutError.h"
#include "Text/MessageCatalog.h"

#include <cmath>
#include <format>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::uint16_t SdCardMenuMessage = 0xA0;
constexpr std::uint16_t HelpMessage = 0xC9;
constexpr std::uint16_t OkMessage = 0x2E;
constexpr std::uint16_t NoCardMessage = 0xA9;
constexpr std::uint16_t UnusableCardMessage = 0xAB;
constexpr std::uint16_t TooManyChannelsMessage = 0xC1;

constexpr std::string_view PageSound = "WSD_SELECT";
constexpr std::string_view ChannelTargetedSound = "WIPL_SE_CH_TARGETTING";
constexpr std::string_view PushSound = "WIPL_SE_BT_PUSH";

// The buttons of the bar, in the order of the button group.
constexpr std::size_t WiiMenuButton = 0;
constexpr std::size_t HelpButton = 1;
constexpr std::size_t NextPageButton = 2;
constexpr std::size_t PreviousPageButton = 3;

constexpr ButtonSet NextPageButtons = {Input::RemoteButtons::Plus, Input::ClassicButtons::Plus};
constexpr ButtonSet PreviousPageButtons = {Input::RemoteButtons::Minus, Input::ClassicButtons::Minus};

// A tile of the list is as big as the icon of a channel.
constexpr float TileWidth = 128;
constexpr float TileHeight = 96;

constexpr std::uint32_t EmptyPlace = 0;

std::string TilePaneName(std::size_t placeOfPage)
{
    return std::format("N_Ch_c{:02}", placeOfPage + 1);
}

SdChannelListScreen* FindList(ScreenManager& screens)
{
    return static_cast<SdChannelListScreen*>(screens.FindScreen(ScreenId::SdChannelList));
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
}

void SdCardMenuBarScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, "mn_SdcardMenu_b");
    layout->SetText("T_page", services.messages.Find(SdCardMenuMessage));
    layout->Calculate();
    buttons = std::make_unique<ButtonGroup>(*layout, services.display, std::vector<std::string>{"B_Wiimenu", "B_Help", "B_ArwR", "B_ArwL"});

    services.screens.OpenChild(ScreenId::SdChannelList, Id(), 0);
    services.fader.StartFadeIn();
}

// The bar is the background of the menu and is drawn below every screen. Its arrows are drawn once more over the list, which is what
// the screen 0x25 of the original does.
void SdCardMenuBarScreen::Draw(DrawPass pass)
{
    if (pass == DrawPass::Underlay)
        layout->Draw();

    if (pass == DrawPass::Overlay)
        DrawArrowsOnly();
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
    bool isActive = !services.dialogs.IsOpen();

    // The help and the notice of too many channels wait for OK and need no answer.
    services.dialogs.TakeAnswer();

    for (const ButtonGroupEvent& event : buttons->Update(services.input, isActive))
    {
        if (event.kind == ButtonGroupEventKind::Focused)
            services.sounds.Request(SoundName::ButtonTargeted);

        if (event.kind != ButtonGroupEventKind::Pressed)
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
            services.sounds.Request(SoundName::Decide);
            DialogRequest request;
            request.layout = DialogLayout::OneButton;
            request.message = services.messages.Find(HelpMessage);
            request.buttonBText = services.messages.Find(OkMessage);
            services.dialogs.Open(request);
        }
        else if (list != nullptr)
        {
            list->TurnPage(event.buttonIndex == NextPageButton ? 1 : -1);
        }
    }
    return false;
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
        std::uint32_t lowPart = places[page * Formats::ChannelPlacesPerPage + placeOfPage];
        if (lowPart == EmptyPlace)
            continue;

        Tile tile;
        tile.placeOfPage = placeOfPage;
        tile.lowPart = lowPart;
        std::optional<ChannelOnCard> channel = ReadChannelOnCard(*services.dataManagement, lowPart);
        try
        {
            if (channel)
                tile.icon = std::make_unique<ChannelIconLayout>(services.display.Device(), channel->icon);
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

bool SdChannelListScreen::RunMain()
{
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

// The hand on a tile puts the cursor around it; A on it asks for the window of the channel (0x813E28B4, 0x813E0450).
void SdChannelListScreen::FollowThePointer()
{
    std::optional<std::size_t> tileUnderPointer = TileUnderPointer();
    if (tileUnderPointer != focusedTile && tileUnderPointer)
        services.sounds.Request(ChannelTargetedSound);

    focusedTile = tileUnderPointer;
    if (!focusedTile || !services.input.AnyPlayerPressed(SelectButtons))
        return;

    services.sounds.Request(PushSound);
    std::size_t place = page * Formats::ChannelPlacesPerPage + tiles[*focusedTile].placeOfPage;
    services.screens.OpenChild(ScreenId::SdChannelWindow, Id(), static_cast<int>(place));
}

std::optional<std::size_t> SdChannelListScreen::TileUnderPointer() const
{
    Formats::Vector2 stretch = services.display.LayoutStretch();
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
        if (!player || !player->state.pointerIsValid)
            continue;

        Formats::Vector2 pointer = services.display.PointerToCanvas(player->state.pointer);
        for (std::size_t tileIndex = 0; tileIndex < tiles.size(); ++tileIndex)
        {
            std::optional<Formats::Vector2> middle = gridLayout->PointOfPaneInPicture(TilePaneName(tiles[tileIndex].placeOfPage), {0, 0});
            bool isInside = middle && std::abs(pointer.x - middle->x) <= TileWidth * stretch.x / 2 && std::abs(pointer.y - middle->y) <= TileHeight / 2;
            if (isInside)
                return tileIndex;
        }
    }
    return std::nullopt;
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
    }

    Formats::Pane& cursor = cursorLayout->PaneNamed("Cursur_a");
    cursor.isVisible = focusedTile.has_value();
    if (!focusedTile)
        return;

    std::optional<Formats::Vector2> middle = gridLayout->PointOfPaneInPicture(TilePaneName(tiles[*focusedTile].placeOfPage), {0, 0});
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

    gridLayout->Draw();
    for (Tile& tile : tiles)
    {
        if (tile.icon)
            tile.icon->Draw();
    }
    cursorLayout->Draw();

    if (isNoticeShown)
        noticeLayout->Draw();
}

}
