#pragma once

#include "Formats/Bytes.h"
#include "Ios/FileSystem/OpenFile.h"
#include "Ios/Result.h"

#include <cstddef>
#include <cstdint>

namespace Ios
{

// A content of an installed title that is open for reading: the file of a program, a banner, a sound.
// The bytes are the ones the title metadata lists: the service has checked their size and SHA-1 hash before it opened the stream.
// A stream has a position. Reading starts there and moves it on. Closing a stream is destroying it.
class ContentStream
{
public:
    explicit ContentStream(Formats::Bytes contentBytes);

    std::uint32_t GetLength() const;
    std::uint32_t GetPosition() const;

    // Reads up to count bytes. Gives back fewer, or none, at the end of the content.
    Formats::Bytes Read(std::size_t count);

    // Moves the position and gives back the new one. The offset may be negative.
    // TitleInvalidArgument when the new position would be before the start or behind the end of the content.
    Result<std::uint32_t> Seek(std::int32_t offset, SeekOrigin origin);

private:
    Formats::Bytes bytes;
    std::uint32_t position = 0;
};

}
