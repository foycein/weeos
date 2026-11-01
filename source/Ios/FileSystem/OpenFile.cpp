#include "Ios/FileSystem/OpenFile.h"

#include "Ios/FileSystem/FileSystemState.h"
#include "Ios/FileSystem/NandLimits.h"

#include <algorithm>
#include <mutex>
#include <utility>

namespace Ios
{

namespace
{

// The console writes a changed block to a new cluster before it lets go of the old one, so even a write that adds no length needs a free cluster.
constexpr std::uint64_t ClustersEveryWriteNeeds = 1;

bool CanRead(OpenMode mode)
{
    return Includes(RightsNeededToOpen(mode), AccessRights::Read);
}

bool CanWrite(OpenMode mode)
{
    return Includes(RightsNeededToOpen(mode), AccessRights::Write);
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

OpenFile::OpenFile(std::shared_ptr<FileSystemState> fileSystemState, NandPath filePath, OpenMode openMode, std::fstream hostStream)
    : state(std::move(fileSystemState))
    , path(std::move(filePath))
    , mode(openMode)
    , stream(std::move(hostStream))
{
}

OpenFile::OpenFile(OpenFile&& other) noexcept
    : state(std::move(other.state))
    , path(std::move(other.path))
    , mode(other.mode)
    , stream(std::move(other.stream))
    , position(other.position)
{
}

OpenFile& OpenFile::operator=(OpenFile&& other) noexcept
{
    if (this == &other)
        return *this;

    if (IsOpen())
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        ReleaseWhileLocked();
    }

    state = std::move(other.state);
    path = std::move(other.path);
    mode = other.mode;
    stream = std::move(other.stream);
    position = other.position;
    return *this;
}

OpenFile::~OpenFile()
{
    if (!IsOpen())
        return;

    std::lock_guard<std::mutex> lock(state->mutex);
    ReleaseWhileLocked();
}

bool OpenFile::IsOpen() const
{
    return state != nullptr;
}

void OpenFile::ReleaseWhileLocked()
{
    stream.close();

    auto openFile = std::find(state->openFilePaths.begin(), state->openFilePaths.end(), path);
    if (openFile != state->openFilePaths.end())
        state->openFilePaths.erase(openFile);
}

Result<Formats::Bytes> OpenFile::Read(std::size_t count)
{
    if (!IsOpen())
        return IosError::InvalidArgument;

    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->isShutDown)
        return IosError::ShutDown;

    if (!CanRead(mode))
        return IosError::AccessDenied;

    std::uint32_t length = state->storage.Find(path)->length;
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

Result<std::size_t> OpenFile::Write(Formats::ByteView data)
{
    if (!IsOpen())
        return IosError::InvalidArgument;

    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->isShutDown)
        return IosError::ShutDown;

    if (!CanWrite(mode))
        return IosError::AccessDenied;

    if (data.empty())
        return std::size_t(0);

    std::uint32_t length = state->storage.Find(path)->length;
    std::uint64_t endPosition = std::uint64_t(position) + data.size();
    std::uint64_t newLength = std::max<std::uint64_t>(length, endPosition);
    if (newLength > LargestFileLength)
        return IosError::NoFreeClusters;

    std::uint64_t newClusterCount = ClustersForLength(newLength) - ClustersForLength(length);
    std::uint64_t neededClusterCount = std::max(newClusterCount, ClustersEveryWriteNeeds);
    if (neededClusterCount > state->FreeClusterCount())
        return IosError::NoFreeClusters;

    stream.clear();
    stream.seekp(position);
    stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    stream.flush();
    if (!stream)
        return IosError::StorageFailure;

    position = static_cast<std::uint32_t>(endPosition);
    state->storage.SetLength(path, static_cast<std::uint32_t>(newLength));
    return data.size();
}

Result<std::uint32_t> OpenFile::Seek(std::int32_t offset, SeekOrigin origin)
{
    if (!IsOpen())
        return IosError::InvalidArgument;

    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->isShutDown)
        return IosError::ShutDown;

    std::uint32_t length = state->storage.Find(path)->length;
    std::int64_t newPosition = SeekBase(origin, position, length) + offset;
    if (newPosition < 0 || newPosition > length)
        return IosError::InvalidArgument;

    position = static_cast<std::uint32_t>(newPosition);
    return position;
}

Result<FileStatistics> OpenFile::GetStatistics() const
{
    if (!IsOpen())
        return IosError::InvalidArgument;

    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->isShutDown)
        return IosError::ShutDown;

    if (!CanRead(mode))
        return IosError::AccessDenied;

    return FileStatistics{state->storage.Find(path)->length, position};
}

Result<void> OpenFile::Close()
{
    if (!IsOpen())
        return IosError::InvalidArgument;

    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->isShutDown)
            return IosError::ShutDown;

        ReleaseWhileLocked();
    }

    state.reset();
    return Result<void>();
}

}
