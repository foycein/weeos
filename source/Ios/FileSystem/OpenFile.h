#pragma once

#include "Formats/Bytes.h"
#include "Ios/FileSystem/EntryPermissions.h"
#include "Ios/FileSystem/NandPath.h"
#include "Ios/Result.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>

namespace Ios
{

struct FileSystemState;

enum class OpenMode : std::uint8_t
{
    Read,
    Write,
    ReadWrite,
};

// The rights a caller needs on a file to open it this way.
constexpr AccessRights RightsNeededToOpen(OpenMode mode)
{
    if (mode == OpenMode::Read)
        return AccessRights::Read;

    if (mode == OpenMode::Write)
        return AccessRights::Write;

    return AccessRights::ReadWrite;
}

// Where a seek counts from.
enum class SeekOrigin : std::uint8_t
{
    Start,
    Current,
    End,
};

struct FileStatistics
{
    std::uint32_t length;
    std::uint32_t position;
};

// A file that is open. It is closed when the object is closed or destroyed, and it can be moved but not copied.
//
// Every open file has a position. Reading and writing start there and move it on. A write at the end makes the file longer.
// The position is never behind the end of the file: a seek there is refused.
//
// Every call gives back ShutDown after the file system was shut down. A call on a file that was closed gives back InvalidArgument.
// Several threads may use their own open files at the same time. A single open file is protected too, but its position is shared by the calls.
class OpenFile
{
public:
    OpenFile(OpenFile&& other) noexcept;
    OpenFile& operator=(OpenFile&& other) noexcept;
    ~OpenFile();

    // Reads up to count bytes. Gives back fewer, or none, at the end of the file.
    // AccessDenied when the file was not opened for reading.
    Result<Formats::Bytes> Read(std::size_t count);

    // Writes the bytes and gives back how many were written. AccessDenied when the file was not opened for writing.
    // NoFreeClusters when the NAND has no room for the new data. Nothing is written then.
    Result<std::size_t> Write(Formats::ByteView data);

    // Moves the position and gives back the new one. The offset may be negative.
    // InvalidArgument when the new position would be before the start or behind the end of the file.
    Result<std::uint32_t> Seek(std::int32_t offset, SeekOrigin origin);

    // The length of the file and the position. As on the console this needs a file that was opened for reading.
    Result<FileStatistics> GetStatistics() const;

    // Closes the file. InvalidArgument when it is closed already.
    // A file that is still open when the file system shuts down stays open: closing it gives back ShutDown.
    Result<void> Close();

private:
    friend class FileSystemService;

    OpenFile(std::shared_ptr<FileSystemState> fileSystemState, NandPath filePath, OpenMode openMode, std::fstream hostStream);

    bool IsOpen() const;

    // Takes the file out of the list of open files and lets go of the file on the PC. The caller holds the mutex of the state.
    void ReleaseWhileLocked();

    std::shared_ptr<FileSystemState> state;
    NandPath path;
    OpenMode mode;
    std::fstream stream;
    std::uint32_t position = 0;
};

}
