#pragma once

#include <compare>
#include <cstdint>
#include <string>

namespace Ios
{

// The number of a title, written "00010002-48414341".
// The high half names the kind of title: 00000001 is the system (the System Menu and the IOS versions), 00010001 a downloaded channel,
// 00010002 a system channel, 00010008 a hidden system channel. The low half names the title itself.
struct TitleId
{
    std::uint32_t highPart = 0;
    std::uint32_t lowPart = 0;

    static TitleId FromValue(std::uint64_t value);
    std::uint64_t ToValue() const;

    // Eight hexadecimal digits, a dash, eight hexadecimal digits.
    std::string ToString() const;

    auto operator<=>(const TitleId&) const = default;
};

constexpr std::uint32_t SystemTitleHighPart = 0x00000001;

constexpr TitleId SystemMenuTitleId = {SystemTitleHighPart, 0x00000002};

// boot2 is 00000001-00000001, the System Menu 00000001-00000002 and the IOS versions 00000001-00000003 to 00000001-000000FF.
// The console does not let a program delete any of them.
constexpr std::uint32_t FirstSystemSoftwareLowPart = 0x00000001;
constexpr std::uint32_t LastSystemSoftwareLowPart = 0x000000FF;

constexpr bool IsSystemSoftware(TitleId titleId)
{
    return titleId.highPart == SystemTitleHighPart
        && titleId.lowPart >= FirstSystemSoftwareLowPart
        && titleId.lowPart <= LastSystemSoftwareLowPart;
}

}
