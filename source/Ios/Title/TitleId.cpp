#include "Ios/Title/TitleId.h"

#include "Formats/NandTitles.h"

namespace Ios
{

namespace
{

constexpr int BitsPerHalf = 32;

}

TitleId TitleId::FromValue(std::uint64_t value)
{
    TitleId titleId;
    titleId.highPart = static_cast<std::uint32_t>(value >> BitsPerHalf);
    titleId.lowPart = static_cast<std::uint32_t>(value);
    return titleId;
}

std::uint64_t TitleId::ToValue() const
{
    return (static_cast<std::uint64_t>(highPart) << BitsPerHalf) | lowPart;
}

std::string TitleId::ToString() const
{
    return Formats::FormatTitleId(ToValue());
}

}
