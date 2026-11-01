#include "Channels/Internet/OperaData.h"

#include "Browser/TextConversion.h"
#include "SystemMenu/IosNandFiles.h"

#include "Formats/FormatError.h"

#include <algorithm>
#include <charconv>
#include <format>

namespace Channels
{

namespace
{

constexpr std::string_view VolumePath = "/title/00010001/48414445/data/opera.vff";
constexpr std::string_view FavoritesFileName = "opera6.adr";
constexpr std::string_view PreferencesFileName = "operarc";
constexpr std::string_view UserPreferencesSection = "[User Prefs]";
constexpr std::string_view SearchEngineKey = "Search Engine";
constexpr std::string_view ToolbarModeKey = "Toolbar Mode";

constexpr std::size_t LinesPerFavorite = 6;

// opera.vff is 1.5 MB, as the original's (the dump's /title/00010001/48414445/data/opera.vff), which makes it a FAT12 volume.
constexpr std::uint32_t VolumeSize = 1572864;

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
        favorite.title = Browser::TextOfUtf8(lines[first]);
        favorite.address = std::string(lines[first + 1]);
        favorite.position = NumberOf(lines[first + 3]);
        favorite.number = NumberOf(lines[first + 4]);
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
    std::optional<Formats::Bytes> stored = files->ReadFile(VolumePath);
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
        files->WriteFile(VolumePath, volume.FileBytes());
    }

    std::optional<Formats::Bytes> favoriteFile = volume.ReadFile(FavoritesFileName);
    if (favoriteFile)
        favorites = ReadFavoriteList(*favoriteFile);

    std::optional<Formats::Bytes> preferenceFile = volume.ReadFile(PreferencesFileName);
    if (preferenceFile)
        preferences = std::string(TextOf(*preferenceFile));
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

    Favorite favorite{title, address, 1, 1};
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
    files->WriteFile(VolumePath, volume.FileBytes());
}

}
