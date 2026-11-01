#pragma once

#include "Formats/Bytes.h"

#include <optional>
#include <string_view>

namespace SystemMenu
{

// The files of the NAND that the menu reads and deletes, named by their paths on the console, for example "/shared2/expired".
class NandFiles
{
public:
    NandFiles() = default;
    NandFiles(const NandFiles&) = delete;
    NandFiles& operator=(const NandFiles&) = delete;
    virtual ~NandFiles() = default;

    // All the bytes of the file. Nothing when the file is not there. Throws a MenuError when it is there and cannot be read.
    virtual std::optional<Formats::Bytes> ReadFile(std::string_view path) = 0;

    // Deletes the file. Nothing happens when it is not there. Throws a MenuError when it is there and cannot be deleted.
    virtual void DeleteFile(std::string_view path) = 0;

    // Makes the file anew with these bytes, readable and writable by everybody, as the menu makes its files. Throws a MenuError when
    // the NAND does not take it. The old file stays as it was until the new one is complete (the safe save of the NAND library).
    virtual void WriteFile(std::string_view path, Formats::ByteView bytes) = 0;
};

}
