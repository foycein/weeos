#include "Ios/FileSystem/HostNames.h"

#include <algorithm>
#include <cctype>
#include <format>

namespace Ios
{

namespace
{

constexpr std::string_view CharactersWindowsRejects = "<>:\"/\\|?*";
constexpr char EscapeMarker = '^';
constexpr char FirstPrintableCharacter = ' ';
constexpr char LastPrintableCharacter = '~';

constexpr std::string_view ReservedDeviceNames[] =
{
    "CON", "PRN", "AUX", "NUL",
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
};

char ToUpperCase(char character)
{
    return static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
}

char ToLowerCase(char character)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
}

bool IsRejectedCharacter(char character)
{
    if (character < FirstPrintableCharacter || character > LastPrintableCharacter)
        return true;

    return CharactersWindowsRejects.find(character) != std::string_view::npos;
}

bool IsIndexFileName(std::string_view name)
{
    return name == IndexFileName || name == IndexTemporaryFileName;
}

// Windows treats "con" and "con.txt" alike, so only the part before the first dot counts.
bool IsReservedDeviceName(std::string_view name)
{
    std::string_view stem = name.substr(0, name.find('.'));
    while (!stem.empty() && stem.back() == ' ')
        stem.remove_suffix(1);

    std::string upperCaseStem;
    for (char character : stem)
        upperCaseStem += ToUpperCase(character);

    return std::find(std::begin(ReservedDeviceNames), std::end(ReservedDeviceNames), upperCaseStem) != std::end(ReservedDeviceNames);
}

bool EqualsIgnoringCase(std::string_view first, std::string_view second)
{
    return std::equal(first.begin(), first.end(), second.begin(), second.end(),
        [](char left, char right) { return ToLowerCase(left) == ToLowerCase(right); });
}

bool IsTaken(std::string_view name, const std::vector<std::string>& siblingHostNames)
{
    return std::any_of(siblingHostNames.begin(), siblingHostNames.end(),
        [&](const std::string& siblingHostName) { return EqualsIgnoringCase(name, siblingHostName); });
}

std::string EscapedCharacter(char character)
{
    return std::format("{}{:02X}", EscapeMarker, static_cast<unsigned char>(character));
}

std::string EscapeHostName(std::string_view nandName)
{
    std::string escaped;
    for (char character : nandName)
    {
        if (IsRejectedCharacter(character))
            escaped += EscapedCharacter(character);
        else
            escaped += character;
    }

    if (escaped.back() == '.' || escaped.back() == ' ')
    {
        char lastCharacter = escaped.back();
        escaped.pop_back();
        escaped += EscapedCharacter(lastCharacter);
    }

    if (IsReservedDeviceName(escaped) || IsIndexFileName(escaped))
        escaped.insert(escaped.begin(), EscapeMarker);

    return escaped;
}

}

bool IsValidHostName(std::string_view name)
{
    if (name.empty() || IsIndexFileName(name))
        return false;

    if (std::any_of(name.begin(), name.end(), IsRejectedCharacter))
        return false;

    if (name.back() == '.' || name.back() == ' ')
        return false;

    return !IsReservedDeviceName(name);
}

std::string ChooseHostName(std::string_view nandName, const std::vector<std::string>& siblingHostNames)
{
    std::string baseName = IsValidHostName(nandName) ? std::string(nandName) : EscapeHostName(nandName);

    std::string hostName = baseName;
    for (int number = 1; IsTaken(hostName, siblingHostNames); ++number)
        hostName = baseName + EscapeMarker + std::to_string(number);

    return hostName;
}

}
