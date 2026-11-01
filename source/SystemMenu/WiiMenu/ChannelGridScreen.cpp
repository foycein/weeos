#include "SystemMenu/WiiMenu/ChannelGridScreen.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MessageBoard/MessageBoardData.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/WiiMenu/BottomBarScreen.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"

#include "Formats/Ash0.h"
#include "Formats/U8Archive.h"
#include "Text/MessageCatalog.h"

#include <algorithm>
#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view SelectionBundle = "chanSel";
constexpr std::string_view DiscBundle = "diskThum";
constexpr std::string_view DamagedIconFile = "corrupt_icon.ash";

constexpr std::string_view StripLayout = "my_IplTop_a";
constexpr std::string_view StripAnimation = "my_IplTop_a";

// The strip slides one page to the left in frames 0 to 20 of its animation and one page to the right in frames 40 to 60.
// Frame 0 is the strip at rest.
constexpr float FrameAtRest = 0;
constexpr float FirstFrameToPreviousPage = 0;
constexpr float LastFrameToPreviousPage = 20;
constexpr float FirstFrameToNextPage = 40;
constexpr float LastFrameToNextPage = 60;

// The televisions fly away to make room for the Message Board in frames 70 to 90, and fly in from it in frames 100 to 120
// (0x813AD980, 0x813AB6C4).
constexpr float FirstFrameOfLeaving = 70;
constexpr float LastFrameOfLeaving = 90;
constexpr float FirstFrameOfComingBack = 100;
constexpr float LastFrameOfComingBack = 120;
constexpr int ComingFromTheBoardArgument = 1;

// The columns of pages of the strip, from the left, and which page each shows, counted from the page that shows.
constexpr std::array<char, 5> ColumnLetters = {'a', 'b', 'c', 'd', 'e'};
constexpr int PageOffsetOfTheFirstColumn = -2;
constexpr char ColumnOfThePageThatShows = 'c';

// The back of the strip: the sheet behind the televisions. The icons are drawn over it, and the rest of the strip, the sheet with
// the holes of the televisions, over them.
constexpr std::array<std::string_view, 5> BackPanes = {"BaseMask0", "BaseMask1", "BaseMask2", "BaseMask3", "BaseMask4"};
constexpr std::string_view FrontMaskPane = "ChMask";

// On a wide screen the holes of the televisions are wider: the masks take the textures of two pictures that the layout keeps for it.
constexpr std::string_view WideMaskPicture = "ChangeTex16x9";
constexpr std::string_view WideEdgePicture = "Picture_16";
constexpr std::array<std::string_view, 5> MaskPictures = {"Picture_00", "Picture_01", "Picture_02", "Picture_03", "Picture_04"};
constexpr std::array<std::string_view, 5> EdgePictures = {"Edge0", "Edge1", "Edge2", "Edge3", "Edge4"};

// A television of the grid is 128 by 96 units, the size of the panes of its places.
constexpr float TelevisionWidth = 128;
constexpr float TelevisionHeight = 96;

// The clock stands below the page that shows and below its neighbors, so that it slides with the strip.
constexpr std::array<std::string_view, 3> ClockPanes = {"N_Clock0", "N_Clock1", "N_Clock2"};

// The message "Disc Channel".
constexpr std::uint16_t DiscChannelMessage = 0;

// A channel is pressed with A, of the Wii Remote or of the Classic Controller. B held down while A goes down picks the channel up.
constexpr ButtonSet PressButtons = {Input::RemoteButtons::A, Input::ClassicButtons::A};
constexpr ButtonSet PickUpHoldButtons = {Input::RemoteButtons::B, Input::ClassicButtons::B};

// Plus turns to the next page and Minus to the one before; on the Classic Controller also R and L.
constexpr ButtonSet NextPageButtons = {Input::RemoteButtons::Plus, Input::ClassicButtons::Plus | Input::ClassicButtons::RightTrigger};
constexpr ButtonSet PreviousPageButtons = {Input::RemoteButtons::Minus, Input::ClassicButtons::Minus | Input::ClassicButtons::LeftTrigger};

// The sounds of the grid, by their names in the sound archive (docs/research/SystemMenuStructure.md 4.3).
constexpr std::string_view TargetingSound = "WIPL_SE_CH_TARGETTING";
constexpr std::string_view BalloonSound = "WIPL_SE_BALLOON";
constexpr std::string_view PageSound = "WSD_SELECT";
constexpr std::string_view PressSound = "WIPL_SE_BT_PUSH";
constexpr std::string_view HoldSound = "WIPL_SE_CH_HOLD";
constexpr std::string_view SetSound = "WIPL_SE_CH_SET";
constexpr std::string_view NotMovedSound = "WIPL_SE_CH_NOT_MOVE";
constexpr std::string_view DecideSound = "WIPL_SE_DECIDE";
constexpr std::string_view StartSound = "WIPL_SE_WII_START";

// A screen that is built is in the tree two steps after the request. A request for a screen that is not built is dropped instead,
// and the grid goes on after this many steps.
constexpr int StepsToWaitForTheStartWindow = 3;

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

std::size_t TileNumber(SlotPosition position)
{
    return static_cast<std::size_t>(position.page * SlotsPerPage + position.index);
}

std::unique_ptr<Layout::LayoutArchive> OpenArchiveOfFile(const MenuFiles& files, std::string_view path)
{
    Formats::U8Archive archive = Formats::ReadU8Archive(Formats::DecompressAsh0(files.ReadFile(path)));
    return std::make_unique<Layout::LayoutArchive>(std::move(archive));
}

}

ChannelGridScreen::ChannelGridScreen(MenuServices& menuServices, int argument)
    : Screen(ScreenId::ChannelGrid)
    , services(menuServices)
{
    if (argument == ComingFromTheBoardArgument)
        state = GridState::ComingFromTheBoard;
}

void ChannelGridScreen::Load()
{
    selectionArchive = services.files.OpenLayoutBundle(SelectionBundle);
    discArchive = services.files.OpenLayoutBundle(DiscBundle);
    damagedIconArchive = OpenArchiveOfFile(services.files, DamagedIconFile);
}

void ChannelGridScreen::Build()
{
    strip = std::make_unique<MenuLayout>(services.display, *selectionArchive, std::string(StripLayout));
    stripSlide = std::make_unique<FramePlayer>(*strip, *selectionArchive, StripAnimation);
    stripSlide->Show(FrameAtRest);
    if (state == GridState::ComingFromTheBoard)
        stripSlide->PlayOnce(FirstFrameOfComingBack, LastFrameOfComingBack);
    if (services.display.GetScreenShape() == Platform::ScreenShape::SixteenByNine)
        UseTheWideMasks();

    ListTheSlotsOfTheStrip();
    MakeTheTiles();
    clock = std::make_unique<ClockDisplay>(services, *selectionArchive);
    CalculateEverything();
    services.wiiMenu.console.UpdateSharedFlags();
}

void ChannelGridScreen::ListTheSlotsOfTheStrip()
{
    int pageOffset = PageOffsetOfTheFirstColumn;
    for (char column : ColumnLetters)
    {
        for (int slotIndex = 0; slotIndex < SlotsPerPage; ++slotIndex)
        {
            std::string paneName = std::format("N_Ch_{}{:02}", column, slotIndex + 1);
            if (strip->Live().FindPane(paneName) != nullptr)
                stripSlots.push_back({paneName, pageOffset, slotIndex});
        }
        ++pageOffset;
    }
}

void ChannelGridScreen::UseTheWideMasks()
{
    for (std::string_view maskPicture : MaskPictures)
        CopyFirstTexture(*strip, maskPicture, WideMaskPicture);

    for (std::string_view edgePicture : EdgePictures)
        CopyFirstTexture(*strip, edgePicture, WideEdgePicture);
}

void ChannelGridScreen::MakeTheTiles()
{
    for (int page = 0; page < PageCount; ++page)
    {
        for (int index = 0; index < SlotsPerPage; ++index)
        {
            SlotPosition position = {page, index};
            tiles[TileNumber(position)] = MakeTile(position);
        }
    }

    TileArchives archives = {*selectionArchive, *discArchive, *damagedIconArchive};
    emptyPlace = ChannelTile::MakeEmptySlot(services.display, archives);
}

std::unique_ptr<ChannelTile> ChannelGridScreen::MakeTile(SlotPosition position)
{
    TileArchives archives = {*selectionArchive, *discArchive, *damagedIconArchive};
    const SavedSlot& slot = services.wiiMenu.channels.SlotAt(position);

    if (slot.kind == SavedSlotKind::DiscChannel)
        return ChannelTile::MakeDiscChannel(services.display, archives, services.messages.Find(DiscChannelMessage));

    if (slot.kind != SavedSlotKind::Title)
        return ChannelTile::MakeEmptySlot(services.display, archives);

    const LoadedChannel& channel = services.wiiMenu.channels.LoadChannel(position);
    if (!channel.icon)
        return ChannelTile::MakeDamagedChannel(services.display, archives);

    const std::u16string& name = channel.names[Formats::EnglishLanguageNumber].firstLine;
    return ChannelTile::MakeChannel(services.display, archives, channel, name);
}

GridState ChannelGridScreen::State() const
{
    return state;
}

int ChannelGridScreen::CurrentPage() const
{
    return services.wiiMenu.channels.CurrentPage();
}

const ChannelTile& ChannelGridScreen::TileAt(SlotPosition position) const
{
    return *tiles.at(TileNumber(position));
}

ChannelTile& ChannelGridScreen::Tile(SlotPosition position)
{
    return *tiles.at(TileNumber(position));
}

Formats::Vector2 ChannelGridScreen::PlaceOfSlot(int slotIndex)
{
    PlacedPanes placedPanes(*strip);
    std::string paneName = std::format("N_Ch_{}{:02}", ColumnOfThePageThatShows, slotIndex + 1);
    return placedPanes.PlaceOf(paneName).value_or(Formats::Vector2{0, 0});
}

// The first time the grid appears in the run of the program, the menu greets with a sound and starts its music.
void ChannelGridScreen::OnStart()
{
    ShowTheArrowsOfThePage();
    if (services.wiiMenu.hasMusicStarted)
        return;

    services.sounds.Request(StartSound);
    services.sounds.StartMusic();
    services.wiiMenu.hasMusicStarted = true;
}

bool ChannelGridScreen::RunMain()
{
    switch (state)
    {
    case GridState::Idle:
        RunIdle();
        break;

    case GridState::TurningToPreviousPage:
    case GridState::TurningToNextPage:
        RunTurningOfThePage();
        break;

    case GridState::HoldingChannel:
        RunHolding();
        break;

    case GridState::WaitingForStartWindow:
        RunWaitingForStartWindow();
        break;

    case GridState::LeavingForWiiOptions:
        if (RunLeaving())
            return true;
        break;

    case GridState::LeavingForTheBoard:
        if (!stripSlide->IsPlaying())
            return true;
        break;

    case GridState::ComingFromTheBoard:
        if (!stripSlide->IsPlaying())
        {
            stripSlide->Show(FrameAtRest);
            state = GridState::Idle;
        }
        break;
    }

    CalculateEverything();
    return false;
}

void ChannelGridScreen::CalculateEverything()
{
    stripSlide->Step();
    strip->Calculate();
    clock->Calculate();
    for (std::unique_ptr<ChannelTile>& tile : tiles)
    {
        bool hasBalloonAppeared = tile->Calculate();
        if (hasBalloonAppeared)
            services.sounds.Request(BalloonSound);
    }

    emptyPlace->Calculate();
}

// The grid answers to the pointers only while the picture is fully in view.
void ChannelGridScreen::RunIdle()
{
    FocusTheTilesUnderThePointers();
    if (services.fader.GetStatus() != FaderStatus::Clear)
        return;

    if (HandlePageButtons())
        return;

    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        std::optional<Formats::Vector2> place = PointerPlaceOf(playerIndex);
        if (place && HandlePressOfPlayer(playerIndex, *place))
            return;
    }
}

void ChannelGridScreen::FocusTheTilesUnderThePointers()
{
    std::array<bool, SlotsPerPage> isPointedAt = {};
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        std::optional<Formats::Vector2> place = PointerPlaceOf(playerIndex);
        std::optional<int> slotIndex = place ? SlotIndexAt(*place) : std::nullopt;
        if (slotIndex)
            isPointedAt[*slotIndex] = true;
    }

    for (int page = 0; page < PageCount; ++page)
    {
        for (int index = 0; index < SlotsPerPage; ++index)
            FocusTile({page, index}, page == CurrentPage() && isPointedAt[index]);
    }
}

// Only a place that holds something takes the focus.
void ChannelGridScreen::FocusTile(SlotPosition position, bool isPointedAt)
{
    ChannelTile& tile = Tile(position);
    bool canBeFocused = tile.Kind() != TileKind::EmptySlot;
    bool shouldBeFocused = canBeFocused && isPointedAt;
    if (shouldBeFocused && !tile.IsFocused())
        services.sounds.Request(TargetingSound);

    tile.SetFocused(shouldBeFocused);
}

bool ChannelGridScreen::HandlePageButtons()
{
    int page = CurrentPage();
    if (services.input.AnyPlayerPressed(NextPageButtons) && page < PageCount - 1)
    {
        TurnToNextPage();
        return true;
    }

    if (services.input.AnyPlayerPressed(PreviousPageButtons) && page > 0)
    {
        TurnToPreviousPage();
        return true;
    }
    return false;
}

bool ChannelGridScreen::HandlePressOfPlayer(int playerIndex, Formats::Vector2 place)
{
    const PlayerInput& player = *services.input.Player(playerIndex);
    if (!WentDown(player, PressButtons))
        return false;

    std::optional<int> slotIndex = SlotIndexAt(place);
    if (slotIndex)
    {
        SlotPosition position = {CurrentPage(), *slotIndex};
        bool wantsToPickUp = IsHeld(player, PickUpHoldButtons);
        if (wantsToPickUp && services.wiiMenu.channels.CanPickUp(position))
        {
            PickUpChannel(playerIndex, position);
            return true;
        }

        if (!wantsToPickUp && Tile(position).Kind() != TileKind::EmptySlot)
        {
            PressChannel(position);
            return true;
        }
        return false;
    }

    BottomBarScreen* bar = Bar();
    std::optional<BarButton> button = bar ? bar->ButtonAt(place) : std::nullopt;
    if (!button)
        return false;

    HandleBarButton(*button);
    return true;
}

void ChannelGridScreen::HandleBarButton(BarButton button)
{
    int page = CurrentPage();
    switch (button)
    {
    case BarButton::PreviousPage:
        if (page > 0)
            TurnToPreviousPage();
        break;

    case BarButton::NextPage:
        if (page < PageCount - 1)
            TurnToNextPage();
        break;

    case BarButton::WiiButton:
        services.sounds.Request(DecideSound);
        services.fader.StartFadeOut();
        state = GridState::LeavingForWiiOptions;
        break;

    case BarButton::MessageBoardButton:
        LeaveForTheBoard();
        break;

    default:
        break;
    }
}

// The envelope: the bar takes the look of the Message Board and the televisions fly away (0x813B0444, 0x813AD980). When the grid has
// ended, the main frame shows the board.
void ChannelGridScreen::LeaveForTheBoard()
{
    if (services.messageBoard == nullptr)
        return;

    services.sounds.Request(DecideSound);
    if (BottomBarScreen* bar = Bar())
    {
        bar->ShowPageArrows(false, false);
        bar->Change(BarChange::ToTheBoard);
    }
    for (std::unique_ptr<ChannelTile>& tile : tiles)
        tile->SetFocused(false);

    stripSlide->PlayOnce(FirstFrameOfLeaving, LastFrameOfLeaving);
    services.messageBoard->isComingFromTheChannels = true;
    state = GridState::LeavingForTheBoard;
}

void ChannelGridScreen::TurnToPreviousPage()
{
    stateAfterTurning = state == GridState::HoldingChannel ? GridState::HoldingChannel : GridState::Idle;
    services.sounds.Request(PageSound);
    stripSlide->PlayOnce(FirstFrameToPreviousPage, LastFrameToPreviousPage);
    state = GridState::TurningToPreviousPage;
}

void ChannelGridScreen::TurnToNextPage()
{
    stateAfterTurning = state == GridState::HoldingChannel ? GridState::HoldingChannel : GridState::Idle;
    services.sounds.Request(PageSound);
    stripSlide->PlayOnce(FirstFrameToNextPage, LastFrameToNextPage);
    state = GridState::TurningToNextPage;
}

// When the strip has slid one page, the page after it shows in the middle. The strip goes back to rest and shows the new page there.
void ChannelGridScreen::RunTurningOfThePage()
{
    if (stripSlide->IsPlaying())
        return;

    int pageChange = state == GridState::TurningToNextPage ? 1 : -1;
    services.wiiMenu.channels.ShowPage(CurrentPage() + pageChange);
    stripSlide->Show(FrameAtRest);
    ShowTheArrowsOfThePage();
    state = stateAfterTurning;
}

void ChannelGridScreen::PickUpChannel(int playerIndex, SlotPosition position)
{
    holdingPlayer = playerIndex;
    heldChannel = position;
    Tile(position).SetFocused(false);
    services.cursors.SetHandGrabbing(playerIndex, true);
    services.sounds.Request(HoldSound);
    state = GridState::HoldingChannel;
}

// The channel follows the hand while A stays down. On the arrows of the bar the pages turn under it.
void ChannelGridScreen::RunHolding()
{
    const std::optional<PlayerInput>& player = services.input.Player(holdingPlayer);
    if (!player || !IsHeld(*player, PressButtons))
    {
        DropChannel();
        return;
    }

    std::optional<Formats::Vector2> place = PointerPlaceOf(holdingPlayer);
    BottomBarScreen* bar = Bar();
    std::optional<BarButton> button = place && bar ? bar->ButtonAt(*place) : std::nullopt;
    if (button == BarButton::PreviousPage)
        TurnToPreviousPage();
    else if (button == BarButton::NextPage)
        TurnToNextPage();
}

// The channel lands on the empty place under the hand. Anywhere else it goes back to where it was.
void ChannelGridScreen::DropChannel()
{
    std::optional<Formats::Vector2> place = PointerPlaceOf(holdingPlayer);
    std::optional<int> slotIndex = place ? SlotIndexAt(*place) : std::nullopt;
    services.cursors.SetHandGrabbing(holdingPlayer, false);
    state = GridState::Idle;
    std::optional<SlotPosition> target;
    if (slotIndex)
        target = SlotPosition{CurrentPage(), *slotIndex};

    if (!target || !services.wiiMenu.channels.CanMove(heldChannel, *target))
    {
        services.sounds.Request(NotMovedSound);
        return;
    }

    services.sounds.Request(SetSound);
    if (*target == heldChannel)
        return;

    services.wiiMenu.channels.MoveChannel(heldChannel, *target);
    std::swap(tiles[TileNumber(heldChannel)], tiles[TileNumber(*target)]);
}

// The original zooms the television of the channel to the whole picture and then opens the channel start window (screen 0x03).
// The window is the next work item; until it is built the request is dropped and the grid goes on.
void ChannelGridScreen::PressChannel(SlotPosition position)
{
    services.wiiMenu.pressedChannel = position;
    services.sounds.Request(PressSound);
    services.sounds.StopMusic(MusicStopStepsOfAChannel);
    services.screens.OpenChild(ScreenId::ChannelStartWindow, ScreenId::ChannelGrid, 0);
    stepsWaited = 0;
    state = GridState::WaitingForStartWindow;
}

void ChannelGridScreen::RunWaitingForStartWindow()
{
    ++stepsWaited;
    if (stepsWaited < StepsToWaitForTheStartWindow)
        return;

    state = GridState::Idle;
    services.sounds.StartMusic();
}

bool ChannelGridScreen::RunLeaving()
{
    if (services.fader.GetStatus() != FaderStatus::Covered)
        return false;

    GoToRoot(ScreenId::WiiOptionsBackground, 0);
    return true;
}

void ChannelGridScreen::ShowTheArrowsOfThePage()
{
    BottomBarScreen* bar = Bar();
    if (bar == nullptr)
        return;

    int page = CurrentPage();
    bar->ShowPageArrows(page > 0, page < PageCount - 1);
}

// A channel hangs at the hand also while the page turns under it.
bool ChannelGridScreen::IsHoldingAChannel() const
{
    bool isTurning = state == GridState::TurningToPreviousPage || state == GridState::TurningToNextPage;
    return state == GridState::HoldingChannel || (isTurning && stateAfterTurning == GridState::HoldingChannel);
}

std::optional<int> ChannelGridScreen::SlotIndexAt(Formats::Vector2 place)
{
    PlacedPanes placedPanes(*strip);
    for (const StripSlot& slot : stripSlots)
    {
        bool isOnThePageThatShows = slot.pageOffset == 0;
        if (isOnThePageThatShows && placedPanes.IsOnPane(slot.paneName, place))
            return slot.slotIndex;
    }
    return std::nullopt;
}

std::optional<Formats::Vector2> ChannelGridScreen::PointerPlaceOf(int playerIndex) const
{
    const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
    if (!player || !player->state.pointerIsValid)
        return std::nullopt;

    return services.display.PointerToCanvas(player->state.pointer);
}

BottomBarScreen* ChannelGridScreen::Bar()
{
    return dynamic_cast<BottomBarScreen*>(services.screens.FindScreen(ScreenId::BottomBar));
}

void ChannelGridScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main || !strip)
        return;

    PlacedPanes placedPanes(*strip);
    auto isBackPane = [](const Formats::Pane& pane) { return std::find(BackPanes.begin(), BackPanes.end(), pane.name) != BackPanes.end(); };
    auto isFrontMask = [](const Formats::Pane& pane) { return pane.name == FrontMaskPane; };
    auto isMiddlePane = [&](const Formats::Pane& pane) { return !isBackPane(pane) && !isFrontMask(pane); };

    DrawPanes(*strip, services.display, placedPanes, isBackPane);
    DrawTiles(placedPanes);
    DrawPanes(*strip, services.display, placedPanes, isMiddlePane);

    for (std::string_view clockPane : ClockPanes)
    {
        std::optional<Formats::Vector2> place = placedPanes.PlaceOf(clockPane);
        if (place)
            clock->DrawAt(*place);
    }

    std::optional<Formats::Vector2> handPlace = PointerPlaceOf(holdingPlayer);
    if (IsHoldingAChannel() && handPlace)
        Tile(heldChannel).DrawAt(*handPlace);

    for (const StripSlot& slot : stripSlots)
    {
        std::optional<Formats::Vector2> place = placedPanes.PlaceOf(slot.paneName);
        if (slot.pageOffset == 0 && place)
            Tile({CurrentPage(), slot.slotIndex}).DrawBalloonAt(*place);
    }

    DrawPanes(*strip, services.display, placedPanes, isFrontMask);
}

void ChannelGridScreen::DrawTiles(const PlacedPanes& placedPanes)
{
    bool isHolding = IsHoldingAChannel();
    for (const StripSlot& slot : stripSlots)
    {
        int page = CurrentPage() + slot.pageOffset;
        if (page < 0 || page >= PageCount)
            continue;

        std::optional<Formats::Vector2> place = placedPanes.PlaceOf(slot.paneName);
        if (!place)
            continue;

        SlotPosition position = {page, slot.slotIndex};
        bool isTheHeldChannel = isHolding && position == heldChannel;
        services.display.Device().SetScissor(AreaOfTheTelevisionAt(*place));
        if (isTheHeldChannel)
            emptyPlace->DrawAt(*place);
        else
            Tile(position).DrawAt(*place);
    }
    services.display.Device().ClearScissor();
}

// A tile draws only inside its television. Some icons have parts beside the picture, which their own script moves into view.
Graphics::ScissorRectangle ChannelGridScreen::AreaOfTheTelevisionAt(Formats::Vector2 place) const
{
    Formats::Vector2 stretch = services.display.LayoutStretch();
    float halfWidth = TelevisionWidth / 2 * stretch.x;
    float halfHeight = TelevisionHeight / 2 * stretch.y;

    Graphics::ScissorRectangle area;
    area.left = place.x - halfWidth;
    area.right = place.x + halfWidth;
    area.bottom = place.y - halfHeight;
    area.top = place.y + halfHeight;
    return area;
}

}
