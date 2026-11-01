#pragma once

#include "Formats/Bytes.h"
#include "Formats/VirtualFatVolume.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Settings/ConsoleClock.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{
class IosNandFiles;
}

namespace Channels
{

// The original keeps at most this many favorites (operarc: "Max Number Favorites = 56"; the manual: "up to 56 Webpages").
constexpr std::size_t MaximumFavoriteCount = 56;

struct Favorite
{
    std::u16string title;
    std::string address;

    // The place in the list, from 1, and the number that names the favorite. Both are written as "%f" numbers.
    int position = 0;
    int number = 0;
};

// The values of the preference "Search Engine" (searchEngineSettingFlow.js: google "1", yahoo "2"). None is set until the first search.
enum class SearchEngine
{
    None = 0,
    Google = 1,
    Yahoo = 2,
};

// The values of the preference "Toolbar Mode" (preferences/menu.js, the order of the options).
enum class ToolbarMode
{
    AlwaysShown = 0,
    HiddenUntilPointedAt = 1,
    ToggledWithOne = 2,
};

// The favorites list, the file opera6.adr of the original: six lines for each favorite, the title, the address, the nickname (always
// empty), the position, the number and the number again, the numbers written as "%f" (default_bookmarks/us_en/append1.adr).
std::vector<Favorite> ReadFavoriteList(Formats::ByteView text);
Formats::Bytes WriteFavoriteList(const std::vector<Favorite>& favorites);

// The value of a preference of an operarc file ("Key=Value" in the section [User Prefs]), and the file with a value changed.
std::optional<std::string> ReadPreference(std::string_view operarc, std::string_view key);
std::string WritePreference(std::string_view operarc, std::string_view key, std::string_view value);

// What the Internet Channel keeps in the NAND: the volume opera.vff in its data folder with the favorites (opera6.adr) and the
// preferences (operarc), as the original keeps them. A console whose channel never ran has no volume: the channel makes it with the
// default favorites and the default preferences of its title.
class OperaData
{
public:
    // The default files come from the channel's own contents: default_bookmarks/us_en/append1.adr and operarc.nand. The clock gives the
    // files of the volume their time.
    OperaData(Ios::FileSystemService& fileSystem, Ios::Caller caller, Formats::ByteView defaultFavorites, Formats::ByteView defaultPreferences,
        Settings::PcClock clock);
    ~OperaData();

    OperaData(const OperaData&) = delete;
    OperaData& operator=(const OperaData&) = delete;

    const std::vector<Favorite>& Favorites() const;
    bool IsFull() const;

    // Adds the page as the last favorite and saves the list. False when the list is full.
    bool AddFavorite(const std::u16string& title, const std::string& address);
    void RemoveFavorite(int number);
    std::optional<Favorite> FindFavorite(const std::string& address) const;

    SearchEngine GetSearchEngine() const;
    void SetSearchEngine(SearchEngine engine);
    ToolbarMode GetToolbarMode() const;
    void SetToolbarMode(ToolbarMode mode);

private:
    void SetPreference(std::string_view key, int value);
    int PreferenceNumber(std::string_view key) const;
    void Save();

    std::unique_ptr<SystemMenu::IosNandFiles> files;
    Settings::PcClock clock;
    Formats::VirtualFatVolume volume;
    std::vector<Favorite> favorites;
    std::string preferences;
};

}
