#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/WiiMenu/Balloon.h"
#include "SystemMenu/WiiMenu/ChannelManager.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"

#include "Formats/Vectors.h"

#include <memory>
#include <optional>
#include <string>

namespace SystemMenu
{

// What a place of the channel grid shows.
enum class TileKind
{
    // The icon of a channel, from its banner.
    Channel,

    // A channel whose files cannot be read: the menu's own picture of a damaged channel.
    DamagedChannel,

    // The Disc Channel. The drive of WeeOS stays empty, so it shows the Disc Channel without a disc.
    DiscChannel,

    // An empty place: a television that shows snow.
    EmptySlot,
};

// The layouts that the tiles of the grid are made of. The archives must outlive the tiles.
struct TileArchives
{
    // chanSel.ash: the empty place (my_IplTop_b), the frame of the focus (my_IplTop_d) and the balloon.
    const Layout::LayoutArchive& channelSelection;

    // diskThum.ash: the Disc Channel (my_DiskCh_b).
    const Layout::LayoutArchive& discThumbnail;

    // corrupt_icon.ash: the picture of a damaged channel.
    const Layout::LayoutArchive& damagedIcon;
};

// One place of the channel grid: the picture that fills its television, the frame that shows while a pointer is on it,
// and the balloon with the name of the channel.
class ChannelTile
{
public:
    // The picture of a channel, from the layout archive of its icon. The channel must have an icon and outlive the tile.
    static std::unique_ptr<ChannelTile> MakeChannel(Display& display, const TileArchives& archives, const LoadedChannel& channel,
        const std::u16string& name);

    static std::unique_ptr<ChannelTile> MakeDamagedChannel(Display& display, const TileArchives& archives);
    static std::unique_ptr<ChannelTile> MakeDiscChannel(Display& display, const TileArchives& archives, const std::u16string& name);
    static std::unique_ptr<ChannelTile> MakeEmptySlot(Display& display, const TileArchives& archives);

    TileKind Kind() const;

    // A pointer came onto the place or went away. The frame shows and the balloon appears after a moment.
    void SetFocused(bool isFocused);
    bool IsFocused() const;

    const Balloon* NameBalloon() const;

    // Gives back true when the balloon has begun to grow into view in this step.
    bool Calculate();

    // Draws the picture and the frame with their middle at a place of the picture.
    void DrawAt(Formats::Vector2 place);

    // Draws the balloon below the place.
    void DrawBalloonAt(Formats::Vector2 place);

private:
    ChannelTile(Display& display, TileKind kind, std::unique_ptr<MenuLayout> picture, const TileArchives& archives);

    void PlayAnimation(const Layout::LayoutArchive& archive, const std::string& animationName);
    void PlayScriptAnimation(const Layout::LayoutArchive& archive, const std::string& animationName);
    void AddBalloon(const TileArchives& archives, const std::u16string& name);

    Display& display;
    TileKind kind;
    std::unique_ptr<MenuLayout> picture;
    std::unique_ptr<MenuLayout> focusFrame;
    std::unique_ptr<Balloon> balloon;
    std::unique_ptr<FramePlayer> focusOn;
    std::unique_ptr<FramePlayer> focusOff;
    float lastFrameOfFocusOn = 0;
    float lastFrameOfFocusOff = 0;
    bool isFocused = false;
};

// A layout of a channel (its icon or its banner) holds its words in every language, each language in a group named after it.
// Hides the panes of the other languages, except those that English uses as well.
void ShowOnlyTheEnglishPanes(MenuLayout& layout, const Layout::LayoutArchive& archive, const std::string& layoutName);

}
