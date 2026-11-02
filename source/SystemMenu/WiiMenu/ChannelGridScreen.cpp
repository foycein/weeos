#include "SystemMenu/WiiMenu/ChannelGridScreen.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MessageBoard/BoardInput.h"
#include "SystemMenu/MessageBoard/MessageBoardData.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/WiiMenu/BottomBarScreen.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"

#include "Formats/Ash0.h"
#include "Formats/FormatError.h"
#include "Formats/U8Archive.h"
#include "Layout/LayoutError.h"
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
// Frame 0 is the strip at rest. While the grid zooms into a television, ChMask darkens the whole grid in frames 200 to 228.
constexpr float FrameAtRest = 0;
constexpr float FirstFrameOfTheDarkening = 200;
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

// Half the size of a television of the grid, in the units of the picture (the constructor 0x813AA4CC takes them from a table by the
// shape of the screen): 170 by 96 units on the 16:9 screen (128 by 96 on a 4:3 screen, which WeeOS does not show).
constexpr float HalfWidthOfAWideTelevision = 85;
constexpr float HalfHeightOfATelevision = 48;

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

// The sounds of the grid, by their names in the sound archive.
constexpr std::string_view TargetingSound = "WIPL_SE_CH_TARGETTING";
constexpr std::string_view BalloonSound = "WIPL_SE_BALLOON";
constexpr std::string_view PageSound = "WSD_SELECT";
constexpr std::string_view PressSound = "WIPL_SE_BT_PUSH";
constexpr std::string_view ZoomSound = "WIPL_SE_CH_SELECT";
constexpr std::string_view HoldSound = "WIPL_SE_CH_HOLD";
constexpr std::string_view SetSound = "WIPL_SE_CH_SET";
constexpr std::string_view NotMovedSound = "WIPL_SE_CH_NOT_MOVE";
constexpr std::string_view DecideSound = "WIPL_SE_DECIDE";
constexpr std::string_view StartSound = "WIPL_SE_WII_START";

// While the window of a channel covers the picture, the grid paints it black (0x813AB0E8).
constexpr Formats::Rgba8Color Black = {0, 0, 0, 255};

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
    emptyPlace = ChannelTile::MakeEmptySlot(services.display, archives, services.wiiMenu.random.Next());
}

std::unique_ptr<ChannelTile> ChannelGridScreen::MakeTile(SlotPosition position)
{
    TileArchives archives = {*selectionArchive, *discArchive, *damagedIconArchive};
    const SavedSlot& slot = services.wiiMenu.channels.SlotAt(position);

    if (slot.kind == SavedSlotKind::DiscChannel)
        return ChannelTile::MakeDiscChannel(services.display, archives, services.messages.Find(DiscChannelMessage));

    if (slot.kind != SavedSlotKind::Title)
        return ChannelTile::MakeEmptySlot(services.display, archives, services.wiiMenu.random.Next());

    const LoadedChannel& channel = services.wiiMenu.channels.LoadChannel(position);
    if (!channel.icon)
        return ChannelTile::MakeDamagedChannel(services.display, archives);

    // Some channels of homebrew tools, which an imported Wii can bring, have icons that Nintendo's layout engine does not know:
    // another name of the layout, or animations of texture patterns. They show as damaged channels.
    const std::u16string& name = channel.names[Formats::EnglishLanguageNumber].firstLine;
    try
    {
        return ChannelTile::MakeChannel(services.display, archives, channel, name, services.wiiMenu.random.Next());
    }
    catch (const Formats::FormatError&)
    {
        return ChannelTile::MakeDamagedChannel(services.display, archives);
    }
    catch (const Layout::LayoutError&)
    {
        return ChannelTile::MakeDamagedChannel(services.display, archives);
    }
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

    case GridState::ZoomingIntoChannel:
    case GridState::ZoomingOutOfChannel:
        RunZoom();
        break;

    case GridState::ShowingChannel:
        break;

    case GridState::LeavingTheWiiMenu:
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
    if (!HasGoneDown(player, PressButtons))
        return false;

    std::optional<int> slotIndex = SlotIndexAt(place);
    if (slotIndex)
    {
        SlotPosition position = {CurrentPage(), *slotIndex};
        bool wantsToPickUp = IsHeldDown(player, PickUpHoldButtons);
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
        {
            Bar()->FlashPageArrow(button);
            TurnToPreviousPage();
        }
        break;

    case BarButton::NextPage:
        if (page < PageCount - 1)
        {
            Bar()->FlashPageArrow(button);
            TurnToNextPage();
        }
        break;

    case BarButton::WiiButton:
        LeaveFor(ScreenId::WiiOptionsBackground);
        break;

    case BarButton::SdCardButton:
        Bar()->PressSdCardButton();
        LeaveFor(ScreenId::SdCardMenuBar);
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
    if (!player || !IsHeldDown(*player, PressButtons))
    {
        DropChannel();
        return;
    }

    std::optional<Formats::Vector2> place = PointerPlaceOf(holdingPlayer);
    BottomBarScreen* bar = Bar();
    std::optional<BarButton> button;
    if (place && bar)
        button = bar->ButtonAt(*place);

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

// The channel start window (screen 0x03) opens as a child of the grid; the arrows of the bar slide away (0x813ADA94). The original waits
// until the icons of the pages around are loaded, which they always are in WeeOS.
void ChannelGridScreen::PressChannel(SlotPosition position)
{
    services.wiiMenu.pressedChannel = position;
    Tile(position).ShowPressed();
    SetBarButtonsEnabled(false);
    services.sounds.Request(PressSound);
    services.sounds.StopMusic(MusicStopStepsOfAChannel);
    Tile(position).SetFocused(false);
    BottomBarScreen* bar = Bar();
    if (bar != nullptr)
        bar->ShowPageArrows(false, false);

    services.screens.OpenChild(ScreenId::ChannelStartWindow, ScreenId::ChannelGrid, 0);
    state = GridState::ZoomingIntoChannel;
}

void ChannelGridScreen::ZoomIntoPressedChannel()
{
    zoom = std::make_unique<TelevisionZoom>(services.display.WholePicture(), TelevisionOfThePressedChannel());
    zoom->StartZoomingIn();
    ShowTheZoom();
    services.sounds.Request(ZoomSound);
}

// The window may have changed to a channel of another page meanwhile: the grid shows that page and zooms out of its television
// (0x813ADC8C, 0x813ADCE8).
void ChannelGridScreen::ZoomOutOfPressedChannel()
{
    SlotPosition channel = services.wiiMenu.pressedChannel.value_or(SlotPosition{});
    services.wiiMenu.channels.ShowPage(channel.page);

    zoom = std::make_unique<TelevisionZoom>(services.display.WholePicture(), TelevisionOfThePressedChannel());
    zoom->StartZoomingOut();
    ShowTheZoom();
    state = GridState::ZoomingOutOfChannel;
}

PictureArea ChannelGridScreen::TelevisionOfThePressedChannel()
{
    int slotIndex = services.wiiMenu.pressedChannel.value_or(SlotPosition{}).index;
    return AreaOfTheTelevisionAt(PlaceOfSlot(slotIndex));
}

// The screens of the Wii Menu look through the camera of the zoom, and the grid darkens as much as the zoom has come.
void ChannelGridScreen::ShowTheZoom()
{
    services.display.SetCamera(zoom->VisibleArea());
    stripSlide->Show(FirstFrameOfTheDarkening + static_cast<float>(zoom->StepsTowardsTheTelevision()));
}

const TelevisionZoom* ChannelGridScreen::Zoom() const
{
    return zoom.get();
}

bool ChannelGridScreen::UpdatesWithChildren() const
{
    return true;
}

bool ChannelGridScreen::DrawsWithChildren() const
{
    return true;
}

// When the grid has zoomed out again and the window is gone, the grid is as it was before the press, and the music of the menu
// starts again (0x813AC7D8).
void ChannelGridScreen::RunZoom()
{
    if (!zoom)
        return;

    zoom->Step();
    ShowTheZoom();
    if (zoom->IsMoving())
        return;

    if (state == GridState::ZoomingIntoChannel)
    {
        state = GridState::ShowingChannel;
        return;
    }

    if (HasChildren())
        return;

    zoom.reset();
    services.display.ResetCamera();
    stripSlide->Show(FrameAtRest);
    state = GridState::Idle;
    SetBarButtonsEnabled(true);
    ShowTheArrowsOfThePage();
    services.sounds.StartMusic();
}

// The camera belongs to the grid: whatever comes after it sees the whole picture again.
void ChannelGridScreen::OnClose()
{
    services.display.ResetCamera();
}

// The bar stops answering, the menu says yes, and the picture fades out before the new screen comes (0x813B0444, 0x813B0728).
void ChannelGridScreen::LeaveFor(ScreenId nextScreen)
{
    Bar()->SetButtonsEnabled(false);
    services.sounds.Request(DecideSound);
    services.fader.StartFadeOut();
    screenAfterLeaving = nextScreen;
    state = GridState::LeavingTheWiiMenu;
}

bool ChannelGridScreen::RunLeaving()
{
    if (services.fader.GetStatus() != FaderStatus::Covered)
        return false;

    GoToRoot(screenAfterLeaving, 0);
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

void ChannelGridScreen::SetBarButtonsEnabled(bool shouldBeEnabled)
{
    BottomBarScreen* bar = Bar();
    if (bar != nullptr)
        bar->SetButtonsEnabled(shouldBeEnabled);
}

void ChannelGridScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main || !strip)
        return;

    if (state == GridState::ShowingChannel)
    {
        services.display.FillArea(services.display.WholePicture(), Black);
        return;
    }

    services.display.LookThroughCamera();
    DrawTheGrid();
    services.display.StopMovingAreas();
}

void ChannelGridScreen::DrawTheGrid()
{
    PlacedPanes placedPanes(*strip);
    auto isBackPane = [](const Formats::Pane& pane) { return std::find(BackPanes.begin(), BackPanes.end(), pane.name) != BackPanes.end(); };
    auto isFrontMask = [](const Formats::Pane& pane) { return pane.name == FrontMaskPane; };
    auto isMiddlePane = [&](const Formats::Pane& pane) { return !isBackPane(pane) && !isFrontMask(pane); };

    // The strip holds the pages beside the one that shows; on a screen wider than 16:9 they go on beside the picture, the televisions
    // of the next page in their holes, so the sheet of the grid continues instead of ending at the edge.
    Graphics::GraphicsDevice& device = services.display.Device();
    device.SetScissor(device.WholeArea());
    DrawPanes(*strip, services.display, placedPanes, isBackPane);
    DrawTiles(placedPanes);
    device.SetScissor(device.WholeArea());
    DrawPanes(*strip, services.display, placedPanes, isMiddlePane);
    device.ClearScissor();

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

// A tile draws only inside its television (0x813ADFDC), also while the grid zooms. Some icons have parts beside the picture, which
// their own script moves into view.
PictureArea ChannelGridScreen::AreaOfTheTelevisionAt(Formats::Vector2 place) const
{
    PictureArea area;
    area.left = place.x - HalfWidthOfAWideTelevision;
    area.right = place.x + HalfWidthOfAWideTelevision;
    area.bottom = place.y - HalfHeightOfATelevision;
    area.top = place.y + HalfHeightOfATelevision;
    return area;
}

}
