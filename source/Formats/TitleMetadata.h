#pragma once

#include "Formats/Bytes.h"
#include "Formats/Sha1.h"
#include "Formats/Signature.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Formats
{

enum class Region : std::uint16_t
{
    Japan = 0,
    Usa = 1,
    Europe = 2,
    RegionFree = 3,
    Korea = 4,
};

// One content (one .app file) of a title, as the title metadata lists it.
struct ContentRecord
{
    std::uint32_t id;
    std::uint16_t index;
    std::uint16_t type;
    std::uint64_t size;
    Sha1Digest hash;
};

constexpr std::size_t TitleRatingsSize = 16;
constexpr std::size_t TitleInputOutputMaskSize = 12;

// The title metadata (TMD) describes a title: who it is, which system software it needs and which contents it has.
struct TitleMetadata
{
    Signature signature;
    std::string issuer;
    std::uint8_t version;
    std::uint8_t certificateAuthorityRevocationVersion;
    std::uint8_t signerRevocationVersion;
    bool isVirtualWiiTitle;

    // The IOS the title runs on. It is zero for IOS itself.
    std::uint64_t requiredSystemVersion;

    std::uint64_t titleId;
    std::uint32_t titleType;
    std::uint16_t groupId;
    Region region;
    std::array<Byte, TitleRatingsSize> ratings;
    std::array<Byte, TitleInputOutputMaskSize> inputOutputMask;
    std::uint32_t accessRights;
    std::uint16_t titleVersion;
    std::uint16_t bootIndex;
    std::vector<ContentRecord> contents;
};

TitleMetadata ReadTitleMetadata(ByteView bytes);

}
