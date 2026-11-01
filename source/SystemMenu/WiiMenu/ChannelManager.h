#pragma once

#include "SystemMenu/WiiMenu/IconScripts.h"
#include "SystemMenu/WiiMenu/MenuConsole.h"
#include "SystemMenu/WiiMenu/SaveData.h"

#include "Formats/BannerContent.h"
#include "Layout/LayoutArchive.h"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SystemMenu
{

// A place of the channel grid. The places of a page are numbered row by row, four in a row and three rows:
//     0  1  2  3
//     4  5  6  7
//     8  9 10 11
struct SlotPosition
{
    int page = 0;
    int index = 0;

    bool operator==(const SlotPosition&) const = default;
};

constexpr int SlotsPerRow = 4;

// The path of the save file of the menu.
constexpr std::string_view SaveDataPath = "/title/00000001/00000002/data/iplsave.bin";

// A channel whose banner has been read: its name in every language and the layouts of its icon.
struct LoadedChannel
{
    std::array<Formats::ChannelName, Formats::BannerLanguageCount> names;

    // Nothing when the files of the channel are damaged. The grid shows the picture of a damaged channel then.
    std::unique_ptr<Layout::LayoutArchive> icon;

    // The animation that the tile plays, for example "icon". Nothing when the icon has none that the menu starts.
    std::optional<std::string> iconAnimation;

    // True when a script or a module of the channel drives the icon (docs/research/Channels.md 1.6).
    bool isIconDrivenByCode = false;

    // What WeeOS does instead of the script, and the numbers of the animations icon_Rso0 to icon_Rso15 that the icon has for it.
    IconScript iconScript = IconScript::None;
    std::vector<int> scriptAnimationNumbers;
};

// The animation of an icon, as the grid chooses it: an icon that a script drives plays icon_Start, when it has it, and the script
// does the rest. Any other icon plays icon, or icon_Whole when it has no icon. The files are the files of icon.bin.
std::optional<std::string> ChooseIconAnimation(const Formats::U8Archive& iconFiles, bool isIconDrivenByCode);

// The numbers of the animations icon_Rso0 to icon_Rso15 that the files of icon.bin hold.
std::vector<int> ListScriptAnimations(const Formats::U8Archive& iconFiles);

// The channel grid as the menu keeps it (the ChannelManager and the SaveDataManager of the original, docs/research/SystemMenuStructure.md 5.1):
// four pages of twelve places, the page that is shown, and the files of the channels in the places.
// The places are saved in iplsave.bin as soon as they change.
class ChannelManager
{
public:
    // Reads iplsave.bin and makes a new file when there is none or when it is damaged. Brings the places up to date with the titles
    // on the NAND and saves the file when anything changed. The console must stay alive as long as this object.
    explicit ChannelManager(MenuConsole& console);

    int CurrentPage() const;

    // Throws a MenuError for a page that does not exist.
    void ShowPage(int page);

    const SavedSlot& SlotAt(SlotPosition position) const;

    // The number of places that hold the Disc Channel or a title.
    int UsedSlotCount() const;

    // True when all 48 places hold a channel.
    bool IsFull() const;

    // True when the channel at the place may be picked up to move it: a channel title, not the Disc Channel, and only while the grid
    // has an empty place.
    bool CanPickUp(SlotPosition position) const;

    // True when the channel at the first place may move to the second: the second place is empty, or it is the first place itself.
    bool CanMove(SlotPosition from, SlotPosition to) const;

    // Moves the channel to an empty place and saves the file. Throws a MenuError when the move is not allowed.
    void MoveChannel(SlotPosition from, SlotPosition to);

    // The channel at the place starts now (0x813B97FC): the menu opens on its page when it comes back, and a downloaded channel goes to the
    // front of the list of started titles. Saves the file.
    void RememberChannelThatStarts(SlotPosition position);

    // The files of the title at the place, read the first time they are asked for. Throws a MenuError for a place without a title.
    const LoadedChannel& LoadChannel(SlotPosition position);

private:
    void Save();
    std::unique_ptr<LoadedChannel> ReadChannel(Ios::TitleId titleId);

    MenuConsole& console;
    SaveData saveData;
    int currentPage = 0;
    std::map<std::uint64_t, std::unique_ptr<LoadedChannel>> loadedChannels;
};

}
