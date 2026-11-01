#pragma once

#include "Formats/TitleMetadata.h"

#include <cstdint>

namespace Ios
{

// The bits of the type of a content in the title metadata.
constexpr std::uint16_t SharedContentTypeFlag = 0x8000;
constexpr std::uint16_t OptionalContentTypeFlag = 0x4000;

// A shared content is kept once in /shared1 for all the titles that have it. The title metadata names it by its hash.
constexpr bool IsSharedContent(const Formats::ContentRecord& record)
{
    return (record.type & SharedContentTypeFlag) != 0;
}

// The title can be installed without an optional content.
constexpr bool IsOptionalContent(const Formats::ContentRecord& record)
{
    return (record.type & OptionalContentTypeFlag) != 0;
}

// The record of the content with this id, or nullptr when the title has none.
inline const Formats::ContentRecord* FindContentById(const Formats::TitleMetadata& metadata, std::uint32_t contentId)
{
    for (const Formats::ContentRecord& record : metadata.contents)
    {
        if (record.id == contentId)
            return &record;
    }
    return nullptr;
}

}
