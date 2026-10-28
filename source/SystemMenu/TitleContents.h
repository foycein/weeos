#pragma once

#include "Formats/Bytes.h"

#include <cstdint>

namespace SystemMenu
{

// The contents of the System Menu title, the files that its title metadata lists. Each content has an index.
// The menu asks for a content by its index and does not care where the bytes come from.
class TitleContents
{
public:
    TitleContents() = default;
    TitleContents(const TitleContents&) = delete;
    TitleContents& operator=(const TitleContents&) = delete;
    virtual ~TitleContents() = default;

    // All the bytes of the content. Throws a MenuError when the title has no content with this index.
    virtual Formats::Bytes ReadContent(std::uint16_t contentIndex) = 0;
};

}
