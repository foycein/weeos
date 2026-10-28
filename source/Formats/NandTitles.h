#pragma once

#include "Formats/Bytes.h"
#include "Formats/NandFileSystem.h"
#include "Formats/Sha1.h"
#include "Formats/TitleMetadata.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Formats
{

// The two halves of a title id in hexadecimal, for example "00000001-00000002" for the System Menu.
std::string FormatTitleId(std::uint64_t titleId);

// Accepts "00000001-00000002" and "0000000100000002". Throws a FormatError for anything else.
std::uint64_t ParseTitleId(std::string_view text);

// Where the NAND keeps the folder of a title ("/title/00000001/00000002") and its ticket ("/ticket/00000001/00000002.tik").
std::string TitleFolderPath(std::uint64_t titleId);
std::string TicketFilePath(std::uint64_t titleId);

// The titles that are installed in a NAND file system, and where the NAND keeps their files:
//   /title/<high>/<low>/content/title.tmd     the title metadata
//   /title/<high>/<low>/content/<id>.app      a content of the title
//   /ticket/<high>/<low>.tik                  the ticket
//   /shared1/<name>.app                       a content that titles share, named in /shared1/content.map by its SHA-1 hash
// The NAND keeps every content decrypted: the title service decrypts it with the title key when it installs a title.
class NandTitles
{
public:
    // Reads /shared1/content.map. The file system must outlive this object.
    explicit NandTitles(NandFileSystem& nandFileSystem);

    // Every title that has a title metadata file, in the order of the file system.
    std::vector<std::uint64_t> ListTitleIds() const;

    Bytes ReadMetadataBytes(std::uint64_t titleId);
    TitleMetadata ReadMetadata(std::uint64_t titleId);

    // The ticket as stored, or nothing when the NAND has no ticket for the title.
    std::optional<Bytes> ReadTicketBytes(std::uint64_t titleId);

    // The path of the file that holds the content, or nothing when the NAND does not have it.
    std::optional<std::string> FindContentPath(std::uint64_t titleId, const ContentRecord& record) const;

    // Throws a FormatError when the NAND does not have the content, or when its size or SHA-1 hash differs from the title metadata.
    Bytes ReadContent(std::uint64_t titleId, const ContentRecord& record);

private:
    struct SharedContent
    {
        std::string name;
        Sha1Digest hash;
    };

    std::vector<SharedContent> ReadSharedContentMap();

    NandFileSystem& fileSystem;
    std::vector<SharedContent> sharedContents;
};

}
