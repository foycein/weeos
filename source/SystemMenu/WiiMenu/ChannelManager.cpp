#include "SystemMenu/WiiMenu/ChannelManager.h"

#include "SystemMenu/MenuError.h"
#include "SystemMenu/WiiMenu/ChannelPlacement.h"

#include "Formats/FormatError.h"

#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view IconFilePath = "meta/icon.bin";

constexpr std::string_view AnimationsFolder = "arc/anim/";
constexpr std::string_view AnimationExtension = ".brlan";
constexpr std::string_view StartAnimation = "icon_Start";
constexpr std::string_view LoopAnimation = "icon";
constexpr std::string_view WholeAnimation = "icon_Whole";

bool HasAnimation(const Formats::U8Archive& iconFiles, std::string_view animationName)
{
    std::string path = std::string(AnimationsFolder) + std::string(animationName) + std::string(AnimationExtension);
    return iconFiles.FindFile(path).has_value();
}

bool IsPositionInsideTheGrid(SlotPosition position)
{
    bool isPageInside = position.page >= 0 && position.page < PageCount;
    bool isIndexInside = position.index >= 0 && position.index < SlotsPerPage;
    return isPageInside && isIndexInside;
}

std::size_t SlotNumber(SlotPosition position)
{
    return static_cast<std::size_t>(position.page * SlotsPerPage + position.index);
}

void RequireInsideTheGrid(SlotPosition position)
{
    if (!IsPositionInsideTheGrid(position))
        throw MenuError(std::format("The place page {} number {} is not in the channel grid", position.page, position.index));
}

}

std::optional<std::string> ChooseIconAnimation(const Formats::U8Archive& iconFiles, bool isIconDrivenByCode)
{
    if (isIconDrivenByCode && HasAnimation(iconFiles, StartAnimation))
        return std::string(StartAnimation);

    if (isIconDrivenByCode)
        return std::nullopt;

    if (HasAnimation(iconFiles, LoopAnimation))
        return std::string(LoopAnimation);

    if (HasAnimation(iconFiles, WholeAnimation))
        return std::string(WholeAnimation);

    return std::nullopt;
}

std::vector<int> ListScriptAnimations(const Formats::U8Archive& iconFiles)
{
    std::vector<int> numbers;
    for (int number = 0; number < ScriptAnimationCount; ++number)
    {
        if (HasAnimation(iconFiles, ScriptAnimationName(number)))
            numbers.push_back(number);
    }
    return numbers;
}

ChannelManager::ChannelManager(MenuConsole& menuConsole)
    : console(menuConsole)
{
    std::optional<Formats::Bytes> file = console.ReadNandFile(SaveDataPath);
    std::optional<SaveData> savedData;
    if (file)
        savedData = ReadSaveData(*file);

    saveData = savedData ? *savedData : MakeDefaultSaveData();
    PlaceChannels(saveData, console.ListInstalledTitles());

    bool isPageValid = saveData.pageToReturnTo < static_cast<std::uint32_t>(PageCount);
    currentPage = isPageValid ? static_cast<int>(saveData.pageToReturnTo) : 0;
    saveData.pageToReturnTo = 0;

    Formats::Bytes bytes = WriteSaveData(saveData);
    if (!file || bytes != *file)
        console.WriteNandFile(SaveDataPath, bytes);
}

int ChannelManager::CurrentPage() const
{
    return currentPage;
}

void ChannelManager::ShowPage(int page)
{
    if (page < 0 || page >= PageCount)
        throw MenuError(std::format("The page {} is not in the channel grid", page));

    currentPage = page;
}

const SavedSlot& ChannelManager::SlotAt(SlotPosition position) const
{
    RequireInsideTheGrid(position);
    return saveData.slots[SlotNumber(position)];
}

int ChannelManager::UsedSlotCount() const
{
    int usedSlotCount = 0;
    for (const SavedSlot& slot : saveData.slots)
        usedSlotCount += slot.kind == SavedSlotKind::Empty ? 0 : 1;

    return usedSlotCount;
}

bool ChannelManager::IsFull() const
{
    return UsedSlotCount() == SlotCount;
}

bool ChannelManager::CanPickUp(SlotPosition position) const
{
    if (!IsPositionInsideTheGrid(position))
        return false;

    return SlotAt(position).kind == SavedSlotKind::Title && !IsFull();
}

bool ChannelManager::CanMove(SlotPosition from, SlotPosition to) const
{
    if (!IsPositionInsideTheGrid(from) || !IsPositionInsideTheGrid(to))
        return false;

    if (SlotAt(from).kind != SavedSlotKind::Title)
        return false;

    return from == to || SlotAt(to).kind == SavedSlotKind::Empty;
}

void ChannelManager::MoveChannel(SlotPosition from, SlotPosition to)
{
    if (!CanMove(from, to))
        throw MenuError("The channel cannot move there: only a channel title moves, and only to an empty place");

    if (from == to)
        return;

    saveData.slots[SlotNumber(to)] = saveData.slots[SlotNumber(from)];
    saveData.slots[SlotNumber(from)] = SavedSlot();
    Save();
}

const LoadedChannel& ChannelManager::LoadChannel(SlotPosition position)
{
    const SavedSlot& slot = SlotAt(position);
    if (slot.kind != SavedSlotKind::Title)
        throw MenuError(std::format("The place page {} number {} holds no title", position.page, position.index));

    std::unique_ptr<LoadedChannel>& channel = loadedChannels[slot.titleId.ToValue()];
    if (!channel)
        channel = ReadChannel(slot.titleId);

    return *channel;
}

void ChannelManager::RememberChannelThatStarts(SlotPosition position)
{
    saveData.pageToReturnTo = static_cast<std::uint32_t>(position.page);
    PutFirstInTheListOfStartedTitles(saveData, SlotAt(position).titleId);
    Save();
}

void ChannelManager::Save()
{
    console.WriteNandFile(SaveDataPath, WriteSaveData(saveData));
}

// A channel with damaged files stays in the grid. It has no icon, so the grid shows the picture of a damaged channel, as the original does.
std::unique_ptr<LoadedChannel> ChannelManager::ReadChannel(Ios::TitleId titleId)
{
    auto channel = std::make_unique<LoadedChannel>();
    try
    {
        Formats::BannerContent banner = Formats::ReadBannerContent(console.ReadBannerContent(titleId));
        channel->names = banner.names;

        std::optional<Formats::ByteView> iconFile = banner.files.FindFile(IconFilePath);
        if (iconFile)
        {
            Formats::U8Archive iconFiles = Formats::ReadLayoutFile(*iconFile);
            channel->iconAnimation = ChooseIconAnimation(iconFiles, banner.isIconDrivenByCode);
            channel->isIconDrivenByCode = banner.isIconDrivenByCode;
            channel->iconScript = FindIconScript(titleId, banner.isIconDrivenByCode);
            channel->scriptAnimationNumbers = ListScriptAnimations(iconFiles);
            channel->icon = std::make_unique<Layout::LayoutArchive>(std::move(iconFiles));
        }
    }
    catch (const Formats::FormatError&)
    {
        channel->icon.reset();
    }
    catch (const MenuError&)
    {
        channel->icon.reset();
    }
    return channel;
}

}
