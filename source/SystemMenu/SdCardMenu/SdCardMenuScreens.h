#pragma once

#include "SystemMenu/ButtonController.h"
#include "SystemMenu/DataManagement/ChannelIconLayout.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/TelevisionZoom.h"
#include "SystemMenu/WiiMenu/Balloon.h"

#include "Formats/SdCardBackup.h"
#include "Layout/LayoutArchive.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace SystemMenu
{

struct ChannelOnCard;
struct ExplanationPage;
enum class PictureOfAPage;

// The bar of the SD Card Menu (screen 0x24, sdButton.ash): the title, the button that goes back to the Wii Menu, the help button and
// the arrows that turn the pages of the list. It opens the list of the channels below it.
class SdCardMenuBarScreen : public Screen
{
public:
    explicit SdCardMenuBarScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;
    bool UpdatesWithChildren() const override;
    bool DrawsWithChildren() const override;

    // 1 when the point lies on the arrow to the next page, -1 on the arrow to the page before, 0 elsewhere.
    int PageArrowAt(Formats::Vector2 place);

    // The balloons that name the Wii Menu button and the help button while a hand rests on them.
    const Balloon& WiiMenuBalloon() const;
    const Balloon& HelpBalloon() const;

protected:
    bool RunMain() override;
    bool RunClosing() override;

private:
    void DrawArrowsOnly();
    void FollowTheFocusWithTheBalloons(const ButtonEvent& event);
    void CalculateTheBalloons();
    void CalculateBalloon(Balloon& balloon, std::optional<Formats::Vector2> placeOfTheButton);

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<Layout::LayoutArchive> balloonArchive;
    std::unique_ptr<MenuLayout> layout;
    std::unique_ptr<ButtonController> buttons;
    std::unique_ptr<Balloon> wiiMenuBalloon;
    std::unique_ptr<Balloon> helpBalloon;
};

// The channels of the SD card (screen 0x22, sdChanSel.ash), 12 to a page in 20 pages, each in the place that loc.dat keeps for it.
// A notice says when there is no card or the card cannot be used, and the list follows the card when it comes or goes.
// A channel that is pressed asks for its window (screen 0x23), from which it would start.
class SdChannelListScreen : public Screen
{
public:
    explicit SdChannelListScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    // The arrows of the bar and the buttons Plus and Minus turn the pages.
    void TurnPage(int direction);

    // The help button of the bar opens the pages that explain the menu.
    void ShowHelp();

    // A channel hangs at the hand: B held while A went down on it picked it up, and it lands where A is let go.
    bool IsHoldingAChannel() const;

    // The page shown, counted from 0.
    std::size_t Page() const;

    // The low half of the title id of the channel in each place of the page shown, zero for an empty place.
    std::array<std::uint32_t, Formats::ChannelPlacesPerPage> ChannelsOfPage() const;

    // The low half of the title id of the channel that was pressed last.
    std::uint32_t PressedChannel() const;

    // The window of a channel (0x23) asks for the zoom into the television of the pressed channel when it is ready, and for the zoom
    // back when it closes, as the channel start window asks the grid (0x813E0BEC).
    void ZoomIntoPressedChannel();
    void ZoomOutOfPressedChannel();

    // The zoom while a window of a channel is open. Nothing at other times.
    const TelevisionZoom* Zoom() const;

    // The list zooms and paints while the window of a channel is its child.
    bool UpdatesWithChildren() const override;
    bool DrawsWithChildren() const override;

protected:
    bool RunMain() override;

private:
    struct Tile
    {
        std::size_t placeOfPage = 0;
        std::uint32_t lowPart = 0;
        std::unique_ptr<ChannelIconLayout> icon;
        std::unique_ptr<Balloon> balloon;
    };

    void ReadChannels();
    void ShowPage();
    void ShowNotice(std::optional<std::uint16_t> messageNumber);
    std::unique_ptr<Balloon> MakeBalloon(const ChannelOnCard& channel);
    void FollowThePointerWithTheBalloons();
    void PlaceTiles();
    void FollowThePointer();
    void PressChannel(std::size_t tileIndex);
    void PickUpChannel(int playerIndex, std::size_t tileIndex);
    void RunHolding();
    void DropChannel();
    std::optional<std::size_t> TileUnderPointer() const;
    std::optional<std::size_t> TileUnderPointerOf(int playerIndex) const;
    std::optional<std::size_t> PlaceOfPageAt(Formats::Vector2 point) const;
    std::optional<Formats::Vector2> PointerPlaceOf(int playerIndex) const;
    void RunZoom();
    void DrawTheList();
    void ExplainTheMenuTheFirstTime();
    void OpenExplanation(std::span<const ExplanationPage> explanationPages);
    MenuLayout* PictureLayout(PictureOfAPage picture);

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> gridLayout;
    std::unique_ptr<MenuLayout> noticeLayout;
    std::unique_ptr<MenuLayout> cursorLayout;

    // The pictures of the pages that explain the menu: the turning icon of a busy console and the help button.
    std::unique_ptr<MenuLayout> waitIconLayout;
    std::unique_ptr<MenuLayout> helpButtonLayout;
    bool hasCheckedIntro = false;
    bool isShowingIntro = false;
    std::size_t noticeInSlot = 0;
    std::size_t cursorInSlot = 0;

    Formats::ChannelPlaces places = {};
    std::vector<Tile> tiles;
    std::size_t page = 0;
    bool isCardReady = false;
    bool isNoticeShown = false;
    bool hasToldTooManyChannels = false;
    bool hasToldWriteProtection = false;
    std::optional<std::size_t> focusedTile;

    // The channel that hangs at the hand of a player, from its place among all places of the card.
    struct HeldChannel
    {
        int playerIndex = 0;
        std::size_t place = 0;
        std::unique_ptr<ChannelIconLayout> icon;
    };
    std::optional<HeldChannel> heldChannel;
    int arrowUnderTheHand = 0;
    int stepsOnAnArrow = 0;

    // The channel whose window is open, the zoom into its television, and whether the window covers the picture.
    std::optional<std::size_t> pressedTile;
    std::unique_ptr<TelevisionZoom> zoom;
    bool isCoveredByWindow = false;
};

}
