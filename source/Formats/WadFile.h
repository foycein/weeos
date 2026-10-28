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

WadFile ReadWadFile(ByteView bytes);

}
