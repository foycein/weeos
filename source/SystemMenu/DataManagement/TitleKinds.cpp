#include "SystemMenu/DataManagement/TitleKinds.h"

#include <array>
#include <algorithm>

namespace SystemMenu
{

namespace
{

constexpr std::uint32_t DiscTitles = 0x00010000;

constexpr std::array<std::uint32_t, 5> ChannelFamilies = {0x00010000, 0x00010001, 0x00010004, 0x00010006, 0x00010007};

constexpr std::size_t CodeLength = 4;
constexpr int BitsPerCharacter = 8;
constexpr std::uint32_t CharacterMask = 0xFF;

char FirstCharacterOf(std::uint32_t lowPart)
{
    return static_cast<char>(lowPart >> (BitsPerCharacter * (CodeLength - 1)));
}

bool IsCapitalLetterOrDigit(char character)
{
    bool isCapitalLetter = character >= 'A' && character <= 'Z';
    bool isDigit = character >= '0' && character <= '9';
    return isCapitalLetter || isDigit;
}

}

bool IsChannelKind(Ios::TitleId titleId)
{
    bool isOfAChannelFamily = std::find(ChannelFamilies.begin(), ChannelFamilies.end(), titleId.highPart) != ChannelFamilies.end();
    return isOfAChannelFamily && IsCapitalLetterOrDigit(FirstCharacterOf(titleId.lowPart));
}

bool HasSaveDataKind(Ios::TitleId titleId)
{
    if (titleId.highPart == DiscTitles)
        return IsCapitalLetterOrDigit(FirstCharacterOf(titleId.lowPart));

    return IsChannelKind(titleId);
}

std::string BackupFolderName(std::uint32_t lowPart)
{
    std::string name;
    for (std::size_t index = 0; index < CodeLength; ++index)
    {
        int shift = BitsPerCharacter * static_cast<int>(CodeLength - 1 - index);
        name.push_back(static_cast<char>((lowPart >> shift) & CharacterMask));
    }
    return name;
}

std::optional<std::uint32_t> LowPartOfBackupFolder(std::string_view folderName)
{
    if (folderName.size() != CodeLength)
        return std::nullopt;

    std::uint32_t lowPart = 0;
    for (char character : folderName)
        lowPart = (lowPart << BitsPerCharacter) | static_cast<std::uint8_t>(character);

    return lowPart;
}

}
