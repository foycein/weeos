#include "WindowsNames.h"

#include "NandFolderLayout.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace
{

constexpr std::string_view CharactersWindowsRejects = "<>:\"/\\|?*";
constexpr char FirstPrintableCharacter = ' ';
constexpr char LastPrintableCharacter = '~';

constexpr std::string_view ReservedDeviceNames[] =
{
    "con", "prn", "aux", "nul",
    "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
    "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
};

bool IsRejectedCharacter(char character)
{
    if (character < FirstPrintableCharacter || character > LastPrintableCharacter)
        return true;

    return CharactersWindowsRejects.find(character) != std::string_view::npos;
}

// Windows treats "con" and "con.txt" alike, so only the part before the first dot counts.
bool IsReservedDeviceName(std::string_view name)
{
    std::string_view stem = name.substr(0, name.find('.'));
    while (!stem.empty() && stem.back() == ' ')
        stem.remove_suffix(1);

    std::string lowerCaseStem = LowerCaseName(stem);
    return std::find(std::begin(ReservedDeviceNames), std::end(ReservedDeviceNames), lowerCaseStem) != std::end(ReservedDeviceNames);
}

}

std::string LowerCaseName(std::string_view name)
{
    std::string lowerCaseName;
    for (char character : name)
        lowerCaseName += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

    return lowerCaseName;
}

bool CanBeStoredOnWindows(std::string_view name)
{
    if (name.empty() || name == IndexFileName || name == IndexTemporaryFileName)
        return false;

    if (std::any_of(name.begin(), name.end(), IsRejectedCharacter))
        return false;

    if (name.back() == '.' || name.back() == ' ')
        return false;

    return !IsReservedDeviceName(name);
}
