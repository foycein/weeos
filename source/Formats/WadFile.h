#pragma once

#include "Formats/Bytes.h"
#include "Formats/Certificate.h"
#include "Formats/Ticket.h"
#include "Formats/TitleMetadata.h"

#include <vector>

namespace Formats
{

// A WAD file packs a whole title: its certificates, ticket, title metadata and the encrypted contents.
struct WadFile
{
    std::vector<Certificate> certificates;
    Ticket ticket;
    TitleMetadata titleMetadata;

    // One entry for each content record of the title metadata, in the same order, still encrypted.
    std::vector<Bytes> encryptedContents;
};

// The parts of a WAD file as they are stored, without reading them: the title service installs them from these bytes.
// The views point into the bytes of the WAD file, so they are only valid while those exist.
struct WadSections
{
    ByteView certificateChain;
    ByteView ticket;
    ByteView titleMetadata;

    // One for each content record of the title metadata, in the same order, still encrypted and padded to a multiple of 16 bytes.
    std::vector<ByteView> encryptedContents;
};

WadSections ReadWadSections(ByteView bytes);

WadFile ReadWadFile(ByteView bytes);

}
