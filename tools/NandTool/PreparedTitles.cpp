#include "PreparedTitles.h"

#include "Formats/FileBytes.h"
#include "Formats/Ticket.h"
#include "Formats/TitleEncryption.h"
#include "Formats/TitleMetadata.h"
#include "Formats/WadFile.h"

#include <format>

namespace
{

constexpr const char* TitleMetadataFileName = "title.tmd";
constexpr const char* TicketFileName = "ticket.tik";
constexpr const char* CertificatesFileName = "certificates.bin";

std::string ContentFileName(std::uint32_t contentId)
{
    return std::format("{:08x}.app", contentId);
}

}

std::filesystem::path PrepareTitleFromWad(Formats::ByteView wadFile, const Formats::AesKey& commonKey, const std::filesystem::path& wiiFilesFolder)
{
    Formats::WadSections sections = Formats::ReadWadSections(wadFile);
    Formats::TitleMetadata metadata = Formats::ReadTitleMetadata(sections.titleMetadata);
    Formats::AesKey titleKey = Formats::DecryptTitleKey(Formats::ReadTicket(sections.ticket), commonKey);

    std::filesystem::path titleFolder = wiiFilesFolder / std::format("{:016x}", metadata.titleId);
    std::filesystem::create_directories(titleFolder);
    Formats::WriteFileBytes(titleFolder / TitleMetadataFileName, sections.titleMetadata);
    Formats::WriteFileBytes(titleFolder / TicketFileName, sections.ticket);
    Formats::WriteFileBytes(titleFolder / CertificatesFileName, sections.certificateChain);

    for (std::size_t index = 0; index < metadata.contents.size(); ++index)
    {
        const Formats::ContentRecord& record = metadata.contents[index];
        Formats::Bytes content = Formats::DecryptContent(sections.encryptedContents[index], titleKey, record);
        Formats::WriteFileBytes(titleFolder / ContentFileName(record.id), content);
    }

    return titleFolder;
}

Ios::Result<void> InstallPreparedTitle(Ios::TitleService& titles, const std::filesystem::path& titleFolder)
{
    Formats::Bytes metadataBytes = Formats::ReadFileBytes(titleFolder / TitleMetadataFileName);
    Formats::Bytes ticketBytes = Formats::ReadFileBytes(titleFolder / TicketFileName);
    Formats::Bytes certificates = Formats::ReadFileBytes(titleFolder / CertificatesFileName);

    Ios::Result<void> ticketImported = titles.ImportTicket(ticketBytes, certificates);
    if (ticketImported.Failed())
        return ticketImported;

    Ios::Result<void> started = titles.BeginInstallation(metadataBytes, certificates);
    if (started.Failed())
        return started;

    for (const Formats::ContentRecord& record : Formats::ReadTitleMetadata(metadataBytes).contents)
    {
        Formats::Bytes content = Formats::ReadFileBytes(titleFolder / ContentFileName(record.id));
        Ios::Result<void> added = titles.AddDecryptedContent(record.id, content);
        if (added.Failed())
        {
            titles.CancelInstallation();
            return added;
        }
    }

    return titles.FinishInstallation();
}
