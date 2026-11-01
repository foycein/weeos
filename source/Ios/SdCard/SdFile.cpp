#include "Ios/SdCard/SdFile.h"

#include "Ios/SdCard/SdCardState.h"
#include "Ios/SdCard/SdPath.h"

#include <algorithm>
#include <mutex>
#include <utility>

namespace Ios
{

namespace
{

bool CanRead(OpenMode mode)
{
    return mode != OpenMode::Write;
}

bool CanWrite(OpenMode mode)
{
    return mode != OpenMode::Read;
}

std::int64_t SeekBase(SeekOrigin origin, std::uint32_t position, std::uint32_t length)
{
    if (origin == SeekOrigin::Start)
        return 0;

    if (origin == SeekOrigin::Current)
        return position;

    return length;
}

}

SdFile::SdFile(std::shared_ptr<SdCardState> cardState, std::uint32_t openedOnCardNumber, OpenMode openMode,
    std::fstream hostStream, std::uint32_t fileLength)
    : state(std::move(cardState))
    , cardNumber(openedOnCardNumber)
    , mode(openMode)
    , stream(std::move(hostStream))
    , length(fileLength)
{
}

SdFile::SdFile(SdFile&& other) noexcept
    : state(std::move(other.state))
    , cardNumber(other.cardNumber)
    , mode(other.mode)
    , stream(std::move(other.stream))
    , length(other.length)
    , position(other.position)
{
}

SdFile& SdFile::operator=(SdFile&& other) noexcept
{
    if (this == &other)
        return *this;

    if (IsOpen())
        stream.close();

    state = std::move(other.state);
    cardNumber = other.cardNumber;
    mode = other.mode;
    stream = std::move(other.stream);
    length = other.length;
    position = other.position;
    return *this;
}

SdFile::~SdFile()
{
    if (IsOpen())
        stream.close();
}

bool SdFile::IsOpen() const
{
    return state != nullptr;
}

// The caller holds the lock.
bool SdFile::IsOnTheCardInTheSlot() const
{
    return state->isInserted && state->cardNumber == cardNumber;
}

Result<Formats::Bytes> SdFile::Read(std::size_t count)
{
    if (!IsOpen())
        return IosError::InvalidArgument;

    std::lock_guard<std::mutex> lock(state->mutex);
    if (!IsOnTheCardInTheSlot())
        return IosError::NoCardInserted;

    if (!CanRead(mode))
        return IosError::AccessDenied;

    std::size_t byteCount = std::min<std::size_t>(count, length - position);
    Formats::Bytes bytes(byteCount);
    if (byteCount == 0)
        return bytes;

    stream.clear();
    stream.seekg(position);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(byteCount));
    if (!stream)
        return IosError::StorageFailure;

    position += static_cast<std::uint32_t>(byteCount);
    return bytes;
}

Result<std::size_t> SdFile::Write(Formats::ByteView data)
{
    if (!IsOpen())
        return IosError::InvalidArgument;

    std::lock_guard<std::mutex> lock(state->mutex);
    if (!IsOnTheCardInTheSlot())
        return IosError::NoCardInserted;

    if (!CanWrite(mode))
        return IosError::AccessDenied;

    if (data.empty())
        return std::size_t(0);

    std::uint64_t endPosition = std::uint64_t(position) + data.size();
    if (endPosition > MaximumSdFileLength)
        return IosError::NoFreeClusters;

    stream.clear();
    stream.seekp(position);
    stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    stream.flush();
    if (!stream)
        return IosError::StorageFailure;

    position = static_cast<std::uint32_t>(endPosition);
    length = std::max(length, position);
    return data.size();
}

Result<std::uint32_t> SdFile::Seek(std::int32_t offset, SeekOrigin origin)
{
    if (!IsOpen())
        return IosError::InvalidArgument;

    std::lock_guard<std::mutex> lock(state->mutex);
    if (!IsOnTheCardInTheSlot())
        return IosError::NoCardInserted;

    std::int64_t newPosition = SeekBase(origin, position, length) + offset;
    if (newPosition < 0 || newPosition > length)
        return IosError::InvalidArgument;

    position = static_cast<std::uint32_t>(newPosition);
    return position;
}

Result<std::uint32_t> SdFile::GetLength() const
{
    if (!IsOpen())
        return IosError::InvalidArgument;

    std::lock_guard<std::mutex> lock(state->mutex);
    if (!IsOnTheCardInTheSlot())
        return IosError::NoCardInserted;

    return length;
}

Result<void> SdFile::Close()
{
    if (!IsOpen())
        return IosError::InvalidArgument;

    bool wasOnTheCardInTheSlot;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        wasOnTheCardInTheSlot = IsOnTheCardInTheSlot();
        stream.close();
    }

    state.reset();
    if (!wasOnTheCardInTheSlot)
        return IosError::NoCardInserted;

    return Result<void>();
}

}
