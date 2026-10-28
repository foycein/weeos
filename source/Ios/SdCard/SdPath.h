#pragma once

#include "Ios/Result.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Ios
{

// What the FAT file system of an SD card allows.
constexpr std::size_t MaximumSdNameLength = 255;

// A file cannot be longer than this. FAT keeps the length in 32 bits.
constexpr std::uint64_t MaximumSdFileLength = 0xFFFFFFFF;

// A path on the SD card, for example "/private/wii/title/HAZA/data.bin". It always starts at the root of the card.
//
// A name has 1 to 255 bytes (text in UTF-8), is not "." or "..", does not end with a dot or a space and has none of
// the characters \ / : * ? " < > | and no control character. The PC cannot store the names of devices such as "con" or "com1"
// either, so they are refused too. Names are not case sensitive: "DATA.BIN" and "data.bin" are the same entry,
// because the PC does not tell them apart.
class SdPath
{
public:
    static SdPath Root();
    static Result<SdPath> Parse(std::string_view text);

    bool IsRoot() const;

    // The last name. Empty for the root.
    std::string_view Name() const;

    // The directory that holds this entry. The parent of the root is the root.
    SdPath Parent() const;

    std::string ToString() const;

    // Where the entry is on the PC, when the card is the folder cardFolder.
    std::filesystem::path ToHostPath(const std::filesystem::path& cardFolder) const;

private:
    std::vector<std::string> names;
};

}
