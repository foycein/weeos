#pragma once

#include "Formats/Bytes.h"
#include "Ios/FileSystem/OpenFile.h"
#include "Ios/Result.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>

namespace Ios
{

struct SdCardState;

// A file that is open on the SD card. It is closed when the object is closed or destroyed, and it can be moved but not copied.
//
// It works like a file of the NAND (OpenFile): reading and writing start at the position and move it on, a write at the end
// makes the file longer, and a seek past the end is refused.
//
// A file belongs to the card it was opened on. When the card is taken out every call gives back NoCardInserted,
// also when the same card is put in again. A write to a card that is write protected gives back AccessDenied.
class SdFile
{
public:
    SdFile(SdFile&& other) noexcept;
    SdFile& operator=(SdFile&& other) noexcept;
    ~SdFile();

    // Reads up to count bytes. Gives back fewer, or none, at the end of the file. AccessDenied when the file was opened for writing only.
    Result<Formats::Bytes> Read(std::size_t count);

    // Writes the bytes and gives back how many were written. AccessDenied when the file was opened for reading only.
    // NoFreeClusters when the file would become longer than FAT allows.
    Result<std::size_t> Write(Formats::ByteView data);

    // Moves the position and gives back the new one. InvalidArgument when it would be before the start or behind the end of the file.
    Result<std::uint32_t> Seek(std::int32_t offset, SeekOrigin origin);

    Result<std::uint32_t> GetLength() const;

    // Closes the file. InvalidArgument when it is closed already. The file is closed also when the card is gone, but then the answer is NoCardInserted.
    Result<void> Close();

private:
    friend class SdCardService;

    SdFile(std::shared_ptr<SdCardState> cardState, std::uint32_t openedOnCardNumber, OpenMode openMode, std::fstream hostStream, std::uint32_t fileLength);

    bool IsOpen() const;
    bool IsOnTheCardInTheSlot() const;

    std::shared_ptr<SdCardState> state;
    std::uint32_t cardNumber;
    OpenMode mode;
    std::fstream stream;
    std::uint32_t length;
    std::uint32_t position = 0;
};

}
