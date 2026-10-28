#pragma once

#include "Formats/Bytes.h"
#include "Formats/Certificate.h"
#include "Formats/Ticket.h"
#include "Formats/TitleMetadata.h"

#include <vector>

namespace Formats
{

// boot2 as the NAND stores it: a header, then the certificates, the ticket and the title metadata, then the one content.
// The content is the kernel image of boot2: a loader and an ELF file that holds the programs ES and FS.
struct Boot2Image
{
    std::vector<Certificate> certificates;
    Ticket ticket;
    TitleMetadata titleMetadata;

    // The content as stored: still encrypted and padded to a multiple of 16 bytes.
    Bytes encryptedContent;
};

// Reads the bytes from the start of boot2 on. What follows the content (the rest of the last NAND page) is ignored.
Boot2Image ReadBoot2Image(ByteView bytes);

}
