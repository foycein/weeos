#pragma once

#include "Formats/Bytes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Formats
{

// Every record of the file starts at a multiple of this, and the file grows only by whole records.
constexpr std::size_t TitleMetadataBackupAlignment = 0x20;

// The head of a record: the title id, the size of the title metadata and zeros.
constexpr std::size_t TitleMetadataBackupHeaderSize = 0x20;

// The title metadata of a title that the System Menu deleted, kept in /title/00000001/00000002/data/tmds.sys,
// so that the title can be set up again when it comes back from the SD card.
struct TitleMetadataBackup
{
    std::uint64_t titleId;

    // As the title service stores it, with its signature.
    Bytes titleMetadata;
};

// The records one after the other. Throws a FormatError when a record runs past the end of the file.
std::vector<TitleMetadataBackup> ReadTitleMetadataBackups(ByteView file);

// One record: the head, the title metadata and zeros up to the next multiple of the alignment.
Bytes WriteTitleMetadataBackup(const TitleMetadataBackup& backup);

}
