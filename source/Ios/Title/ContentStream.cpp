#include "Ios/Title/ContentStream.h"

#include <algorithm>
#include <utility>

namespace Ios
{

namespace
{

// Where a seek starts counting, as a position in the content.
std::int64_t OriginPosition(SeekOrigin origin, std::uint32_t position, std::uint32_t length)
{
    switch (origin)
    {
    case SeekOrigin::Start:
        return 0;

    case SeekOrigin::Current:
        return position;

    case SeekOrigin::End:
        return length;
    }

    return 0;
}

}

ContentStream::ContentStream(Formats::Bytes contentBytes)
    : bytes(std::move(contentBytes))
{
}

std::uint32_t ContentStream::GetLength() const
{
    return static_cast<std::uint32_t>(bytes.size());
}

std::uint32_t ContentStream::GetPosition() const
{
    return position;
}

Formats::Bytes ContentStream::Read(std::size_t count)
{
    std::size_t remaining = bytes.size() - position;
    std::size_t readCount = std::min(count, remaining);

    auto first = bytes.begin() + position;
    Formats::Bytes readBytes(first, first + readCount);
    position += static_cast<std::uint32_t>(readCount);
    return readBytes;
}

Result<std::uint32_t> ContentStream::Seek(std::int32_t offset, SeekOrigin origin)
{
    std::int64_t newPosition = OriginPosition(origin, position, GetLength()) + offset;

    bool isInside = newPosition >= 0 && newPosition <= GetLength();
    if (!isInside)
        return IosError::TitleInvalidArgument;

    position = static_cast<std::uint32_t>(newPosition);
    return position;
}

}
