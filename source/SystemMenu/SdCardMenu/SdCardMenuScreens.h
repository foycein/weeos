#pragma once

#include "SystemMenu/ButtonGroup.h"
#include "SystemMenu/DataManagement/ChannelIconLayout.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/TelevisionZoom.h"

#include "Formats/SdCardBackup.h"
#include "Layout/LayoutArchive.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace SystemMenu
{

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

protected:
    bool RunMain() override;
    bool RunClosing() override;

private:
    void DrawArrowsOnly();

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    std::unique_ptr<ButtonGroup> buttons;
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
    };

    void ReadChannels();
    void ShowPage();
    void ShowNotice(std::optional<std::uint16_t> messageNumber);
    void PlaceTiles();
    void FollowThePointer();
    std::optional<std::size_t> TileUnderPointer() const;
    void RunZoom();
    void DrawTheList();

    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> gridLayout;
    std::unique_ptr<MenuLayout> noticeLayout;
    std::unique_ptr<MenuLayout> cursorLayout;
    std::size_t noticeInSlot = 0;
    std::size_t cursorInSlot = 0;

    Formats::ChannelPlaces places = {};
    std::vector<Tile> tiles;
    std::size_t page = 0;
    bool isCardReady = false;
    bool isNoticeShown = false;
    bool hasToldTooManyChannels = false;
    std::optional<std::size_t> focusedTile;

    // The channel whose window is open, the zoom into its television, and whether the window covers the picture.
    std::optional<std::size_t> pressedTile;
    std::unique_ptr<TelevisionZoom> zoom;
    bool isCoveredByWindow = false;
};

}
