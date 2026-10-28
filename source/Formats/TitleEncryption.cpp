#include "Formats/TitleEncryption.h"

#include "Formats/FormatError.h"
#include "Formats/Sha1.h"
#include "Formats/WiiKeys.h"

#include <algorithm>
#include <format>

namespace Formats
{

namespace
{

constexpr std::uint8_t CommonKeyIndex = 0;

// Writes the number into the first bytes of the block, most significant byte first.
void StoreBigEndianNumber(std::uint64_t number, std::size_t byteCount, AesBlock& block)
{
    for (std::size_t index = byteCount; index > 0; --index)
    {
        block[index - 1] = static_cast<Byte>(number & ByteMask);
        number >>= BitsPerByte;
    }
}

// The title key is decrypted with the title id in the first eight bytes of the initialization vector.
AesBlock MakeTitleKeyInitializationVector(std::uint64_t titleId)
{
    AesBlock initializationVector = {};
    StoreBigEndianNumber(titleId, sizeof(titleId), initializationVector);
    return initializationVector;
}

// A content is decrypted with its index in the first two bytes of the initialization vector.
AesBlock MakeContentInitializationVector(std::uint16_t contentIndex)
{
    AesBlock initializationVector = {};
    StoreBigEndianNumber(contentIndex, sizeof(contentIndex), initializationVector);
    return initializationVector;
}

}

AesKey DecryptTitleKey(const Ticket& ticket)
{
    if (ticket.commonKeyIndex != CommonKeyIndex)
    {
        throw FormatError(std::format("Ticket: only the common key with index {} is known, but the ticket uses index {}",
            CommonKeyIndex, ticket.commonKeyIndex));
    }

    if (ticket.consoleId != 0)
    {
        throw FormatError(std::format("Ticket: it is bound to the console {:#x}, so its title key cannot be decrypted here",
            ticket.consoleId));
    }

    AesBlock initializationVector = MakeTitleKeyInitializationVector(ticket.titleId);
    Bytes titleKeyBytes = DecryptAes128Cbc(ticket.encryptedTitleKey, WiiCommonKey, initializationVector);

    AesKey titleKey;
    std::copy(titleKeyBytes.begin(), titleKeyBytes.end(), titleKey.begin());
    return titleKey;
}

Bytes DecryptContent(ByteView encryptedContent, const AesKey& titleKey, const ContentRecord& record)
{
    if (encryptedContent.size() < record.size)
    {
        throw FormatError(std::format("Content {:08x}: the title metadata says {} bytes but only {} encrypted bytes are given",
            record.id, record.size, encryptedContent.size()));
    }

    AesBlock initializationVector = MakeContentInitializationVector(record.index);
    Bytes content = DecryptAes128Cbc(encryptedContent, titleKey, initializationVector);
    content.resize(static_cast<std::size_t>(record.size));

    if (ComputeSha1(content) != record.hash)
    {
        throw FormatError(std::format("Content {:08x}: the SHA-1 hash does not match the title metadata, "
                                      "so the key is wrong or the data is damaged", record.id));
    }

    return content;
}

}
