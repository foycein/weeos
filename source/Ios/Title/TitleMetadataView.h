#pragma once

#include "Formats/TitleMetadata.h"
#include "Ios/Title/TitleId.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Ios
{

struct ContentView
{
    std::uint32_t id;
    std::uint16_t index;
    std::uint16_t type;
    std::uint64_t size;

    bool operator==(const ContentView&) const = default;
};

// The compact form of the title metadata, 0x5C bytes and 0x10 for every content in the original.
// It leaves out the signature, the issuer, the access rights, the boot index and the hash of every content.
struct TitleMetadataView
{
    std::uint8_t version;

    // The IOS the title runs on. Zero for an IOS itself.
    TitleId requiredSystemVersion;

    TitleId titleId;
    std::uint32_t titleType;
    std::uint16_t groupId;
    Formats::Region region;
    std::array<Formats::Byte, Formats::TitleRatingsSize> ratings;
    std::array<Formats::Byte, Formats::TitleInputOutputMaskSize> inputOutputMask;
    std::uint16_t titleVersion;
    std::vector<ContentView> contents;

    bool operator==(const TitleMetadataView&) const = default;
};

TitleMetadataView MakeTitleMetadataView(const Formats::TitleMetadata& metadata);

}
