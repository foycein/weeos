#include "Channels/Internet/OperaData.h"

#include "Browser/TextConversion.h"
#include "Channels/Internet/ThumbnailPicture.h"
#include "SystemMenu/IosNandFiles.h"

#include "Formats/FormatError.h"

#include <algorithm>
#include <charconv>
#include <format>

namespace Channels
{

namespace
{

// The data folder of the Internet Channel (00010001-48414445, HADE).
constexpr std::string_view DataFolder = "/title/00010001/48414445/data/";
constexpr std::string_view VolumeFileName = "opera.vff";
constexpr std::string_view FavoritesFileName = "opera6.adr";
constexpr std::string_view PreferencesFileName = "operarc";
constexpr std::string_view UserPreferencesSection = "[User Prefs]";
constexpr std::string_view SearchEngineKey = "Search Engine";
constexpr std::string_view ToolbarModeKey = "Toolbar Mode";

// The lines of a favorite that the channel reads; the nickname and the number again are not read.
constexpr std::size_t LinesPerFavorite = 6;
constexpr std::size_t TitleLine = 0;
constexpr std::size_t AddressLine = 1;
constexpr std::size_t PositionLine = 3;
constexpr std::size_t NumberLine = 4;

// The first favorite of an empty list.
constexpr int FirstPosition = 1;
constexpr int FirstNumber = 1;

// The pictures of the favorites (the channel's data folder of a real console): bmarks.bmp beside opera.vff has 140 places of 18560 bytes,
// each a bitmap of 90 x 68 pixels; BMARKS.TXT in opera.vff has a line for every place, the name of its picture or "*" for a free place.
// The name of a picture is WeeOS's own: the number of the favorite in eight hexadecimal digits (Assumed: the original's names were not
// traced to the favorites).
constexpr std::string_view ThumbnailFileName = "bmarks.bmp";
constexpr std::string_view ThumbnailNameFolder = "bmarks/";
constexpr std::string_view ThumbnailListFileName = "BMARKS.TXT";
constexpr std::size_t ThumbnailPlaceCount = 140;
constexpr std::size_t ThumbnailPlaceSize = 18560;
constexpr std::string_view FreeThumbnailPlace = "*";

// opera.vff is 1.5 MB, as the original's (/title/00010001/48414445/data/opera.vff on a real console), which makes it a FAT12 volume.
constexpr std::uint32_t VolumeSize = 1572864;

std::string PathInDataFolder(std::string_view name)
{
    return std::string(DataFolder) + std::string(name);
}

std::vector<std::string_view> SplitLines(std::string_view text)
{
    std::vector<std::string_view> lines;
    while (!text.empty())
    {
        std::size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        if (line.ends_with('\r'))
            line.remove_suffix(1);
        lines.push_back(line);
        if (end == std::string_view::npos)
            break;
        text.remove_prefix(end + 1);
    }
    return lines;
}

int NumberOf(std::string_view text)
{
    double number = 0;
    std::from_chars(text.data(), text.data() + text.size(), number);
    return static_cast<int>(number);
}

std::string_view TextOf(Formats::ByteView bytes)
{
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

Formats::Bytes BytesOf(std::string_view text)
{
    return Formats::Bytes(text.begin(), text.end());
}

Formats::VolumeTime VolumeTimeOf(std::chrono::local_seconds time)
{
    Settings::CalendarTime calendarTime = Settings::ConsoleClock::ToCalendarTime(time);
    return {calendarTime.year, calendarTime.month, calendarTime.day, calendarTime.hour, calendarTime.minute, calendarTime.second};
}

std::string_view Trimmed(std::string_view text)
{
    while (!text.empty() && text.front() == ' ')
        text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ')
        text.remove_suffix(1);
    return text;
}

bool IsLineOfKey(std::string_view line, std::string_view key)
{
    std::size_t equals = line.find('=');
    return equals != std::string_view::npos && Trimmed(line.substr(0, equals)) == key;
}

}

std::vector<Favorite> ReadFavoriteList(Formats::ByteView text)
{
    std::vector<std::string_view> lines = SplitLines(TextOf(text));

    std::vector<Favorite> favorites;
    for (std::size_t first = 0; first + LinesPerFavorite <= lines.size(); first += LinesPerFavorite)
    {
        Favorite favorite;
        favorite.title = Browser::TextOfUtf8(lines[first + TitleLine]);
        favorite.address = std::string(lines[first + AddressLine]);
        favorite.position = NumberOf(lines[first + PositionLine]);
        favorite.number = NumberOf(lines[first + NumberLine]);
        favorites.push_back(favorite);
    }
    return favorites;
}

Formats::Bytes WriteFavoriteList(const std::vector<Favorite>& favorites)
{
    std::string text;
    for (const Favorite& favorite : favorites)
    {
        text += Browser::Utf8TextOf(favorite.title) + "\n";
        text += favorite.address + "\n";
        text += "\n";
        text += std::format("{:f}\n{:f}\n{:f}\n", static_cast<double>(favorite.position), static_cast<double>(favorite.number),
            static_cast<double>(favorite.number));
    }
    return BytesOf(text);
}

std::optional<std::string> ReadPreference(std::string_view operarc, std::string_view key)
{
    bool isInUserPreferences = false;
    for (std::string_view line : SplitLines(operarc))
    {
        if (line.starts_with('['))
            isInUserPreferences = line == UserPreferencesSection;

        if (isInUserPreferences && IsLineOfKey(line, key))
            return std::string(Trimmed(line.substr(line.find('=') + 1)));
    }
    return std::nullopt;
}

// The line of the key is replaced, or a new line comes right after the line of the section.
std::string WritePreference(std::string_view operarc, std::string_view key, std::string_view value)
{
    std::string newLine = std::format("{}={}\n", key, value);
    bool hasKey = ReadPreference(operarc, key).has_value();
    bool hasSection = operarc.find(UserPreferencesSection) != std::string_view::npos;

    std::string text;
    bool isInUserPreferences = false;
    for (std::string_view line : SplitLines(operarc))
    {
        if (line.starts_with('['))
            isInUserPreferences = line == UserPreferencesSection;

        if (isInUserPreferences && IsLineOfKey(line, key))
        {
            text += newLine;
            continue;
        }

        text += std::string(line) + "\n";
        if (line == UserPreferencesSection && !hasKey)
            text += newLine;
    }

    if (!hasSection)
        text += std::string(UserPreferencesSection) + "\n" + newLine;
    return text;
}

OperaData::OperaData(Ios::FileSystemService& fileSystem, Ios::Caller caller, Formats::ByteView defaultFavorites,
    Formats::ByteView defaultPreferences, Settings::PcClock consoleClock)
    : files(std::make_unique<SystemMenu::IosNandFiles>(fileSystem, caller))
    , clock(std::move(consoleClock))
    , volume(Formats::VirtualFatVolume::MakeEmpty(VolumeSize))
{
    std::optional<Formats::Bytes> stored = files->ReadFile(PathInDataFolder(VolumeFileName));
    bool isReadable = false;
    if (stored)
    {
        try
        {
            volume = Formats::VirtualFatVolume::Read(*stored);
            isReadable = true;
        }
        catch (const Formats::FormatError&)
        {
        }
    }

    // The original makes a new volume when it finds none or a broken one ("Settings, favorites, and browsing data are lost due to file
    // corruption."); WeeOS does the same without the message.
    if (!isReadable)
    {
        Formats::VolumeTime now = VolumeTimeOf(clock());
        volume.WriteFile(FavoritesFileName, defaultFavorites, now);
        volume.WriteFile(PreferencesFileName, defaultPreferences, now);
        files->WriteFile(PathInDataFolder(VolumeFileName), volume.FileBytes());
    }

    std::optional<Formats::Bytes> favoriteFile = volume.ReadFile(FavoritesFileName);
    if (favoriteFile)
        favorites = ReadFavoriteList(*favoriteFile);

    std::optional<Formats::Bytes> preferenceFile = volume.ReadFile(PreferencesFileName);
    if (preferenceFile)
        preferences = std::string(TextOf(*preferenceFile));

    ReadThumbnails();
}

OperaData::~OperaData() = default;

const std::vector<Favorite>& OperaData::Favorites() const
{
    return favorites;
}

bool OperaData::IsFull() const
{
    return favorites.size() >= MaximumFavoriteCount;
}

// The new favorite gets the next number and the next position, as addFavorite of favorites.js.
bool OperaData::AddFavorite(const std::u16string& title, const std::string& address)
{
    if (IsFull())
        return false;

    Favorite favorite{title, address, FirstPosition, FirstNumber};
    for (const Favorite& existing : favorites)
    {
        favorite.position = std::max(favorite.position, existing.position + 1);
        favorite.number = std::max(favorite.number, existing.number + 1);
    }

    favorites.push_back(favorite);
    Save();
    return true;
}

void OperaData::RemoveFavorite(int number)
{
    std::erase_if(favorites, [number](const Favorite& favorite) { return favorite.number == number; });
    Save();

    std::optional<std::size_t> place = ThumbnailPlaceOf(number);
    if (!place)
        return;

    thumbnailNames[*place] = std::string(FreeThumbnailPlace);
    SaveThumbnails();
}

void OperaData::RenameFavorite(int number, const std::u16string& title)
{
    for (Favorite& favorite : favorites)
    {
        if (favorite.number == number)
            favorite.title = title.substr(0, LongestFavoriteTitle);
    }
    Save();
}

void OperaData::MoveFavorite(int number, std::size_t newIndex)
{
    auto found = std::find_if(favorites.begin(), favorites.end(), [number](const Favorite& favorite) { return favorite.number == number; });
    if (found == favorites.end() || newIndex >= favorites.size())
        return;

    Favorite moved = *found;
    favorites.erase(found);
    favorites.insert(favorites.begin() + static_cast<std::ptrdiff_t>(newIndex), moved);

    for (std::size_t index = 0; index < favorites.size(); ++index)
        favorites[index].position = static_cast<int>(index) + 1;
    Save();
}

void OperaData::SetThumbnail(int number, const Formats::RgbaImage& thumbnail)
{
    std::optional<std::size_t> place = ThumbnailPlaceOf(number);
    if (!place)
    {
        auto freePlace = std::find(thumbnailNames.begin(), thumbnailNames.end(), FreeThumbnailPlace);
        if (freePlace == thumbnailNames.end())
            return;
        place = static_cast<std::size_t>(freePlace - thumbnailNames.begin());
    }

    std::string pictureName = std::format("{:08x}.bmp", static_cast<std::uint32_t>(number));
    thumbnailNames[*place] = PathInDataFolder(ThumbnailNameFolder) + pictureName;
    Formats::Bytes bitmap = WriteThumbnailBitmap(thumbnail);
    std::copy(bitmap.begin(), bitmap.end(), thumbnailFile.begin() + static_cast<std::ptrdiff_t>(*place * ThumbnailPlaceSize));
    SaveThumbnails();
}

std::optional<Formats::RgbaImage> OperaData::Thumbnail(int number) const
{
    std::optional<std::size_t> place = ThumbnailPlaceOf(number);
    if (!place)
        return std::nullopt;

    return ReadThumbnailBitmap(Formats::ByteView(thumbnailFile).subspan(*place * ThumbnailPlaceSize, ThumbnailPlaceSize));
}

std::optional<std::size_t> OperaData::ThumbnailPlaceOf(int number) const
{
    std::string name = std::format("/{:08x}.bmp", static_cast<std::uint32_t>(number));
    for (std::size_t place = 0; place < thumbnailNames.size(); ++place)
    {
        if (thumbnailNames[place].ends_with(name))
            return place;
    }
    return std::nullopt;
}

// A console whose channel never kept a picture has neither file: every place is free.
void OperaData::ReadThumbnails()
{
    std::optional<Formats::Bytes> listFile = volume.ReadFile(ThumbnailListFileName);
    if (listFile)
    {
        for (std::string_view line : SplitLines(TextOf(*listFile)))
            thumbnailNames.emplace_back(line);
    }
    thumbnailNames.resize(ThumbnailPlaceCount, std::string(FreeThumbnailPlace));

    std::optional<Formats::Bytes> stored = files->ReadFile(PathInDataFolder(ThumbnailFileName));
    thumbnailFile = stored ? *stored : Formats::Bytes();
    thumbnailFile.resize(ThumbnailPlaceCount * ThumbnailPlaceSize, 0);
}

void OperaData::SaveThumbnails()
{
    std::string list;
    for (const std::string& name : thumbnailNames)
        list += name + "\n";

    volume.WriteFile(ThumbnailListFileName, BytesOf(list), VolumeTimeOf(clock()));
    files->WriteFile(PathInDataFolder(VolumeFileName), volume.FileBytes());
    files->WriteFile(PathInDataFolder(ThumbnailFileName), thumbnailFile);
}

std::optional<Favorite> OperaData::FindFavorite(const std::string& address) const
{
    for (const Favorite& favorite : favorites)
    {
        if (favorite.address == address)
            return favorite;
    }
    return std::nullopt;
}

SearchEngine OperaData::GetSearchEngine() const
{
    int value = PreferenceNumber(SearchEngineKey);
    if (value == static_cast<int>(SearchEngine::Google) || value == static_cast<int>(SearchEngine::Yahoo))
        return static_cast<SearchEngine>(value);

    return SearchEngine::None;
}

void OperaData::SetSearchEngine(SearchEngine engine)
{
    SetPreference(SearchEngineKey, static_cast<int>(engine));
}

ToolbarMode OperaData::GetToolbarMode() const
{
    int value = PreferenceNumber(ToolbarModeKey);
    if (value == static_cast<int>(ToolbarMode::HiddenUntilPointedAt) || value == static_cast<int>(ToolbarMode::ToggledWithOne))
        return static_cast<ToolbarMode>(value);

    return ToolbarMode::AlwaysShown;
}

void OperaData::SetToolbarMode(ToolbarMode mode)
{
    SetPreference(ToolbarModeKey, static_cast<int>(mode));
}

void OperaData::SetPreference(std::string_view key, int value)
{
    preferences = WritePreference(preferences, key, std::to_string(value));
    Save();
}

int OperaData::PreferenceNumber(std::string_view key) const
{
    std::optional<std::string> value = ReadPreference(preferences, key);
    if (!value)
        return 0;

    return NumberOf(*value);
}

void OperaData::Save()
{
    Formats::VolumeTime now = VolumeTimeOf(clock());
    volume.WriteFile(FavoritesFileName, WriteFavoriteList(favorites), now);
    volume.WriteFile(PreferencesFileName, BytesOf(preferences), now);
    files->WriteFile(PathInDataFolder(VolumeFileName), volume.FileBytes());
}

}
