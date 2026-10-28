#pragma once

#include "Formats/Aes128.h"
#include "Formats/Bytes.h"
#include "Formats/Ticket.h"
#include "Formats/TitleMetadata.h"

namespace Formats
{

// Decrypts the title key of a ticket with the common key. Tickets bound to one console are not supported.
AesKey DecryptTitleKey(const Ticket& ticket);

// Decrypts one content of a title, cuts it to the size the title metadata gives
// and checks the SHA-1 hash. A wrong key or damaged data throws a FormatError.
Bytes DecryptContent(ByteView encryptedContent, const AesKey& titleKey, const ContentRecord& record);

}
