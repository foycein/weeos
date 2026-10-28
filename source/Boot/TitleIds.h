#pragma once

#include <cstdint>

namespace Boot
{

constexpr int BitsPerTitleIdPart = 32;

// The System Menu is the title that boot2 starts.
constexpr std::uint64_t SystemMenuTitleId = 0x0000000100000002;

// The titles 00000001-00000003 to 00000001-000000FF are the IOS versions. Each one is a kernel with its modules.
constexpr std::uint64_t FirstIosTitleId = 0x0000000100000003;
constexpr std::uint64_t LastIosTitleId = 0x00000001000000FF;

constexpr std::uint32_t TitleIdHighPart(std::uint64_t titleId)
{
    return static_cast<std::uint32_t>(titleId >> BitsPerTitleIdPart);
}

constexpr std::uint32_t TitleIdLowPart(std::uint64_t titleId)
{
    return static_cast<std::uint32_t>(titleId);
}

constexpr bool IsIosTitleId(std::uint64_t titleId)
{
    return titleId >= FirstIosTitleId && titleId <= LastIosTitleId;
}

}
