#include "SystemMenu/WiiMenu/ChannelPlacement.h"

#include "SystemMenu/WiiMenu/ChannelTitleIds.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>

namespace SystemMenu
{

namespace
{

constexpr std::uint64_t StubBannerSize = 0x40;

// The version of the empty stand-in that the console has instead of the first Photo Channel.
constexpr std::uint16_t StandInVersion = 0xFF00;

// The channels that the menu puts on a new console. A new title of one of these families comes before the other new titles.
constexpr std::array<std::uint32_t, 5> DefaultFamilies = {0x48414300, 0x48414100, 0x48414200, 0x48414600, 0x48414700};

using TitleIds = std::vector<Ios::TitleId>;
using Titles = std::vector<InstalledTitle>;

// The title that fills a place, or nothing for an empty place.
using PlacedTitles = std::array<Ios::TitleId, SlotCount>;

bool IsEmpty(Ios::TitleId titleId)
{
    return titleId.ToValue() == 0;
}

bool IsInFamily(Ios::TitleId titleId, std::uint32_t kind, std::uint32_t family)
{
    return titleId.highPart == kind && (titleId.lowPart & FamilyMask) == family;
}

bool IsFirstPhotoChannel(Ios::TitleId titleId)
{
    return IsInFamily(titleId, SystemChannelKind, FirstPhotoChannelFamily);
}

bool IsPhotoChannel11(Ios::TitleId titleId)
{
    return IsInFamily(titleId, SystemChannelKind, PhotoChannel11Family);
}

bool IsPhotoChannelSave(Ios::TitleId titleId)
{
    return IsInFamily(titleId, GameSaveKind, PhotoChannelSaveFamily);
}

bool IsPhotoChannel(Ios::TitleId titleId)
{
    return IsFirstPhotoChannel(titleId) || IsPhotoChannel11(titleId) || IsPhotoChannelSave(titleId);
}

template <typename Condition>
bool HasTitleInstalledProperly(const Titles& titles, Condition condition)
{
    return std::any_of(titles.begin(), titles.end(), [&](const InstalledTitle& title) { return title.isInstalledProperly && condition(title.id); });
}

template <typename Condition>
void RemoveTitles(Titles& titles, Condition condition)
{
    std::erase_if(titles, [&](const InstalledTitle& title) { return condition(title.id); });
}

bool IsInstalledProperly(const Titles& titles, Ios::TitleId titleId)
{
    return HasTitleInstalledProperly(titles, [&](Ios::TitleId id) { return id == titleId; });
}

// ---- The titles that the grid considers

// A title of a kind that the grid shows, and not an empty stand-in. The stand-in of the first Photo Channel stays for now,
// because its version helps to decide which Photo Channel the grid shows.
bool IsConsideredByTheGrid(const InstalledTitle& title)
{
    if (!IsKindShownInTheGrid(title.id.highPart))
        return false;

    bool isStandIn = title.bannerSize == StubBannerSize;
    return !isStandIn || IsFirstPhotoChannel(title.id);
}

// The first Photo Channel as a channel that works, not only as the empty stand-in that the console has until Photo Channel 1.1 comes.
bool HasRealFirstPhotoChannel(const Titles& titles)
{
    return std::any_of(titles.begin(), titles.end(),
        [](const InstalledTitle& title) { return IsFirstPhotoChannel(title.id) && title.version != StandInVersion && title.isInstalledProperly; });
}

// The Photo Channel exists in three versions, and the grid shows one of them. The later versions win, as the original decides:
//  - Both later versions: the Photo Channel 1.1 (HAY).
//  - Only the one that is a save of a disc (HAZ): that one.
//  - Only Photo Channel 1.1: the first version, when that is a real channel. Otherwise, when the first version is only the empty
//    stand-in or is not installed at all, Photo Channel 1.1. The original shows no Photo Channel at all when the first version
//    is not installed, which no console can come to, because it ships with the stand-in.
//  - No later version: the first version.
// The versions that do not win are taken out of the list. Gives back the winner.
Ios::TitleId ChoosePhotoChannel(Titles& titles)
{
    Ios::TitleId firstVersion = {SystemChannelKind, FirstPhotoChannelFamily};
    Ios::TitleId version11 = {SystemChannelKind, PhotoChannel11Family};
    Ios::TitleId saveVersion = {GameSaveKind, PhotoChannelSaveFamily};

    bool hasVersion11 = HasTitleInstalledProperly(titles, IsPhotoChannel11);
    bool hasSaveVersion = HasTitleInstalledProperly(titles, IsPhotoChannelSave);

    if (hasVersion11 && hasSaveVersion)
    {
        RemoveTitles(titles, [](Ios::TitleId id) { return IsFirstPhotoChannel(id) || IsPhotoChannelSave(id); });
        return version11;
    }

    if (hasSaveVersion)
    {
        RemoveTitles(titles, IsFirstPhotoChannel);
        return saveVersion;
    }

    if (!hasVersion11)
        return firstVersion;

    if (HasRealFirstPhotoChannel(titles))
    {
        RemoveTitles(titles, IsPhotoChannel11);
        return firstVersion;
    }

    RemoveTitles(titles, IsFirstPhotoChannel);
    return version11;
}

// ---- Comparing the title of a place with an installed title

enum class Comparison
{
    SameTitle,
    OtherChannel,

    // The place has a later version and the installed title is the first version, which only stands in for it.
    PlaceHasTheLaterVersion,

    // The place has the first version and the installed title is a later one. It takes the place.
    InstalledTitleIsLater,
};

Comparison Compare(Ios::TitleId placeTitle, Ios::TitleId installedTitle)
{
    if (placeTitle == installedTitle)
        return Comparison::SameTitle;

    if (!AreInOneFamily(placeTitle, installedTitle))
        return Comparison::OtherChannel;

    if (!IsFirstVersion(placeTitle) && IsFirstVersion(installedTitle))
        return Comparison::PlaceHasTheLaterVersion;

    if (IsFirstVersion(placeTitle) && !IsFirstVersion(installedTitle))
        return Comparison::InstalledTitleIsLater;

    return Comparison::OtherChannel;
}

// A place of a Photo Channel asks for the one that the grid shows, in its first version form, whichever one it held before.
Ios::TitleId ReferenceOfPlace(Ios::TitleId savedTitle, Ios::TitleId photoChannel)
{
    if (!IsPhotoChannel(savedTitle) || AreInOneFamily(savedTitle, photoChannel))
        return savedTitle;

    return {photoChannel.highPart, (photoChannel.lowPart & FamilyMask) | FirstVersionLetter};
}

// ---- The steps of the placing

// Every place that holds a title looks for it, or for a later version of it, among the titles that are installed.
// An installed title that a place has used is taken out of the list, also when it is not installed properly: then the place stays empty.
void KeepChannelsOfThePlaces(const SaveData& saveData, const Titles& titles, Ios::TitleId photoChannel, TitleIds& candidates, PlacedTitles& placed)
{
    for (std::size_t index = 0; index < saveData.slots.size(); ++index)
    {
        const SavedSlot& slot = saveData.slots[index];
        if (slot.kind != SavedSlotKind::Title)
            continue;

        Ios::TitleId reference = ReferenceOfPlace(slot.titleId, photoChannel);
        for (Ios::TitleId& candidate : candidates)
        {
            if (IsEmpty(candidate))
                continue;

            Comparison comparison = Compare(reference, candidate);
            if (comparison == Comparison::OtherChannel)
                continue;

            Ios::TitleId chosen = comparison == Comparison::InstalledTitleIsLater ? candidate : reference;
            candidate = {};
            if (!IsInstalledProperly(titles, chosen))
                continue;

            placed[index] = chosen;
            reference = chosen;
        }
    }
}

void AddNewTitle(TitleIds& newTitles, Ios::TitleId title)
{
    for (Ios::TitleId& listed : newTitles)
    {
        Comparison comparison = Compare(listed, title);
        if (comparison == Comparison::SameTitle || comparison == Comparison::PlaceHasTheLaterVersion)
            return;

        if (comparison == Comparison::InstalledTitleIsLater)
        {
            listed = title;
            return;
        }
    }
    newTitles.push_back(title);
}

std::size_t DefaultOrderOf(Ios::TitleId title)
{
    for (std::size_t order = 0; order < DefaultFamilies.size(); ++order)
    {
        if (IsInFamily(title, SystemChannelKind, DefaultFamilies[order]))
            return order;
    }
    return DefaultFamilies.size();
}

// The titles that no place has used, in the order in which they take their places: the channels of the menu's own table come first,
// then the others in the order of the title service. Of two versions of one channel only the later one is listed.
TitleIds ListNewTitles(const Titles& titles, const TitleIds& candidates)
{
    TitleIds newTitles;
    for (Ios::TitleId candidate : candidates)
    {
        if (!IsEmpty(candidate) && IsInstalledProperly(titles, candidate))
            AddNewTitle(newTitles, candidate);
    }

    std::stable_sort(newTitles.begin(), newTitles.end(), [](Ios::TitleId first, Ios::TitleId second) { return DefaultOrderOf(first) < DefaultOrderOf(second); });
    return newTitles;
}

// The first empty place. The place of the Disc Channel is never free.
std::optional<std::size_t> FindFreePlace(const SaveData& saveData, const PlacedTitles& placed)
{
    for (std::size_t index = 0; index < saveData.slots.size(); ++index)
    {
        bool isDisc = saveData.slots[index].kind == SavedSlotKind::DiscChannel;
        if (!isDisc && IsEmpty(placed[index]))
            return index;
    }
    return std::nullopt;
}

void PlaceNewTitles(const SaveData& saveData, const TitleIds& newTitles, PlacedTitles& placed)
{
    for (Ios::TitleId title : newTitles)
    {
        std::optional<std::size_t> place = FindFreePlace(saveData, placed);
        if (!place)
            return;

        placed[*place] = title;
    }
}

// The places of the file take over what was placed: a place with another title is written new, a place without a title is emptied.
// A place that is already right is not touched, so that its flag stays.
void WritePlacedTitles(SaveData& saveData, const PlacedTitles& placed)
{
    for (std::size_t index = 0; index < saveData.slots.size(); ++index)
    {
        SavedSlot& slot = saveData.slots[index];
        if (slot.kind == SavedSlotKind::DiscChannel || slot.titleId == placed[index])
            continue;

        slot = SavedSlot();
        if (!IsEmpty(placed[index]))
        {
            slot.kind = SavedSlotKind::Title;
            slot.itemNumber = TitleItemNumber;
            slot.titleId = placed[index];
        }
    }
}

}

void PlaceChannels(SaveData& saveData, const std::vector<InstalledTitle>& installedTitles)
{
    Titles titles;
    std::copy_if(installedTitles.begin(), installedTitles.end(), std::back_inserter(titles), IsConsideredByTheGrid);

    Ios::TitleId photoChannel = ChoosePhotoChannel(titles);

    TitleIds candidates;
    for (const InstalledTitle& title : titles)
        candidates.push_back(title.id);

    PlacedTitles placed = {};
    KeepChannelsOfThePlaces(saveData, titles, photoChannel, candidates, placed);
    PlaceNewTitles(saveData, ListNewTitles(titles, candidates), placed);
    WritePlacedTitles(saveData, placed);
}

}
