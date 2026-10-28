#include "DumpVerification.h"

#include "Formats/Certificate.h"
#include "Formats/NandTitles.h"

#include <optional>
#include <span>

namespace
{

constexpr const char* CertificateStorePath = "/sys/cert.sys";

// A ticket of format version 0 has 676 bytes. The ticket file of a title with several tickets holds them one after the other.
constexpr std::size_t SignedTicketSize = 0x2A4;

std::vector<Formats::SignatureCheck> CheckTickets(const Formats::Bytes& ticketFile, std::span<const Formats::Certificate> certificates)
{
    std::vector<Formats::SignatureCheck> results;
    for (std::size_t offset = 0; offset + SignedTicketSize <= ticketFile.size(); offset += SignedTicketSize)
    {
        Formats::ByteView ticket = Formats::ByteView(ticketFile).subspan(offset, SignedTicketSize);
        results.push_back(Formats::VerifySignedFile(ticket, certificates));
    }
    return results;
}

}

ErrorCorrectionReport CheckErrorCorrection(Formats::NandImage& image, std::uint32_t step)
{
    ErrorCorrectionReport report;
    for (std::uint32_t pageNumber = 0; pageNumber < Formats::NandPageCount; pageNumber += step)
    {
        switch (image.CheckPageErrorCorrection(pageNumber))
        {
        case Formats::NandPageCheck::Erased:
            ++report.erasedPageCount;
            break;
        case Formats::NandPageCheck::Correct:
            ++report.correctPageCount;
            break;
        case Formats::NandPageCheck::Unprotected:
            report.unprotectedPages.push_back(pageNumber);
            break;
        case Formats::NandPageCheck::Wrong:
            report.wrongPages.push_back(pageNumber);
            break;
        }
    }
    return report;
}

HmacReport CheckHmacs(Formats::NandFileSystem& fileSystem, Formats::ByteView hmacKey, std::uint32_t largestFileSize)
{
    HmacReport report;
    for (const Formats::NandEntry& entry : fileSystem.Entries())
    {
        if (entry.kind != Formats::NandEntryKind::File)
            continue;

        if (entry.size > largestFileSize)
        {
            ++report.skippedFileCount;
            continue;
        }

        ++report.checkedFileCount;
        report.checkedClusterCount += (entry.size + Formats::NandClusterSize - 1) / Formats::NandClusterSize;
        report.wrongClusterCount += fileSystem.CountClustersWithWrongHmac(entry, hmacKey);
    }

    report.superblockIsCorrect = fileSystem.SuperblockHmacIsCorrect(hmacKey);
    return report;
}

std::vector<TitleSignatureReport> CheckSignaturesOfTitles(Formats::NandFileSystem& fileSystem, std::span<const std::uint64_t> titleIds)
{
    std::vector<Formats::Certificate> certificates = Formats::ReadCertificateChain(fileSystem.ReadFile(CertificateStorePath));
    Formats::NandTitles titles(fileSystem);

    std::vector<TitleSignatureReport> reports;
    for (std::uint64_t titleId : titleIds)
    {
        TitleSignatureReport report;
        report.titleId = titleId;
        report.metadata = Formats::VerifySignedFile(titles.ReadMetadataBytes(titleId), certificates);

        std::optional<Formats::Bytes> ticketFile = titles.ReadTicketBytes(titleId);
        if (ticketFile)
            report.tickets = CheckTickets(*ticketFile, certificates);

        reports.push_back(report);
    }
    return reports;
}
