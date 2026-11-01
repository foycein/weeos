#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/TelevisionZoom.h"
#include "SystemMenu/WiiMenu/ChannelManager.h"
#include "SystemMenu/WiiMenu/ChannelTile.h"
#include "SystemMenu/WiiMenu/ClockDisplay.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"
#include "SystemMenu/WiiMenu/LayoutParts.h"

#include "Layout/LayoutArchive.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SystemMenu
{

class BottomBarScreen;
enum class BarButton;

// What the channel grid is doing.
enum class GridState
{
    // Waits for the pointers and the buttons.
    Idle,

    // The strip of pages slides one page to the left or to the right.
    TurningToPreviousPage,
    TurningToNextPage,

    // A channel hangs at the hand of a player, who moves it to an empty place.
    HoldingChannel,

    // A channel was pressed: the channel start window opens as a child of the grid, and the grid zooms into the television of the
    // channel once the window is ready for it.
    ZoomingIntoChannel,

    // The window of the channel covers the picture. The grid paints black under it.
    ShowingChannel,

    // The window closes, and the grid zooms out of the television.
    ZoomingOutOfChannel,

    // The Wii button or the SD card button was pressed: the picture fades out, then the menu goes to Wii Options or to the SD Card Menu.
    LeavingTheWiiMenu,

    // The envelope was pressed: the televisions fly away and the grid ends; the Message Board comes in its place.
    LeavingForTheBoard,

    // The grid comes back from the Message Board: the televisions fly in.
    ComingFromTheBoard,
};

// The channel grid of the Wii Menu (screen 0x02, docs/research/SystemMenuStructure.md 4.3): four pages of twelve televisions on a
// strip that slides sideways, the clock below them, the balloon with the name of a channel, and the hands that press, turn the
// pages and move channels.
//
// A player points at a channel and presses A to open it. Holding B and pressing A picks the channel up; it follows the hand while A
// stays down and lands on the empty place where A is let go. Plus and Minus, and the arrows of the bar, turn the pages.
class ChannelGridScreen : public Screen
{
public:
    // The argument 1 brings the grid back from the Message Board (0x813935D4 opens it so), 0 is the start of the Wii Menu.
    // The original also knows the argument 2, with which the Message Board opens the grid; it is not built.
    ChannelGridScreen(MenuServices& menuServices, int argument);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;
    void OnClose() override;

    GridState State() const;
    int CurrentPage() const;
    const ChannelTile& TileAt(SlotPosition position) const;

    // The place of the picture where the middle of a television of the page that shows lies, in layout units.
    Formats::Vector2 PlaceOfSlot(int slotIndex);

    // The channel start window asks for the zoom into the television of the pressed channel when it is built (0x813ADC08), and for the
    // zoom back out when it closes (0x813ADCE8). While the grid zooms, the camera of the display shows the part of the Wii Menu that
    // the zoom looks at.
    void ZoomIntoPressedChannel();
    void ZoomOutOfPressedChannel();

    // The zoom while a window of a channel is open. Nothing at other times.
    const TelevisionZoom* Zoom() const;

    // The grid zooms and paints while the window of a channel is its child.
    bool UpdatesWithChildren() const override;
    bool DrawsWithChildren() const override;

protected:
    void OnStart() override;
    bool RunMain() override;

private:
    // A television of the strip: the pane where it lies, and which page and place it shows. The strip has five columns of pages:
    // the page before the page before (only its last column), the page before, the page that shows, the page after and the page
    // after that (only its first column), so that the neighbors are in view while the strip slides.
    struct StripSlot
    {
        std::string paneName;
        int pageOffset = 0;
        int slotIndex = 0;
    };

    void ListTheSlotsOfTheStrip();
    void UseTheWideMasks();
    void MakeTheTiles();
    std::unique_ptr<ChannelTile> MakeTile(SlotPosition position);

    void CalculateEverything();
    void RunIdle();
    void RunTurningOfThePage();
    void RunHolding();
    void RunZoom();
    void ShowTheZoom();
    PictureArea TelevisionOfThePressedChannel();
    void DrawTheGrid();
    bool RunLeaving();

    void FocusTheTilesUnderThePointers();
    void FocusTile(SlotPosition position, bool isPointedAt);
    bool HandlePageButtons();
    bool HandlePressOfPlayer(int playerIndex, Formats::Vector2 place);
    void HandleBarButton(BarButton button);
    void LeaveFor(ScreenId nextScreen);
    void LeaveForTheBoard();

    void TurnToPreviousPage();
    void TurnToNextPage();
    void PickUpChannel(int playerIndex, SlotPosition position);
    void DropChannel();
    void PressChannel(SlotPosition position);
    void ShowTheArrowsOfThePage();

    bool IsHoldingAChannel() const;
    std::optional<int> SlotIndexAt(Formats::Vector2 place);
    std::optional<Formats::Vector2> PointerPlaceOf(int playerIndex) const;
    BottomBarScreen* Bar();
    void SetBarButtonsEnabled(bool shouldBeEnabled);
    ChannelTile& Tile(SlotPosition position);
    void DrawTiles(const PlacedPanes& placedPanes);
    PictureArea AreaOfTheTelevisionAt(Formats::Vector2 place) const;

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> selectionArchive;
    std::unique_ptr<Layout::LayoutArchive> discArchive;
    std::unique_ptr<Layout::LayoutArchive> damagedIconArchive;
    std::unique_ptr<MenuLayout> strip;
    std::unique_ptr<FramePlayer> stripSlide;
    std::unique_ptr<ClockDisplay> clock;
    std::vector<StripSlot> stripSlots;
    std::array<std::unique_ptr<ChannelTile>, SlotCount> tiles;

    // What the place of a channel that hangs at the hand shows meanwhile.
    std::unique_ptr<ChannelTile> emptyPlace;

    std::unique_ptr<TelevisionZoom> zoom;

    GridState state = GridState::Idle;
    ScreenId screenAfterLeaving = ScreenId::WiiOptionsBackground;
    GridState stateAfterTurning = GridState::Idle;
    int holdingPlayer = 0;
    SlotPosition heldChannel;
};

}
