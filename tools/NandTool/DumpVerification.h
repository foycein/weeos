#pragma once

#include "Formats/Bytes.h"
#include "Formats/NandFileSystem.h"
#include "Formats/NandImage.h"
#include "Formats/SignatureVerification.h"

#include <cstdint>
#include <span>
#include <vector>

// The optional checks of a NAND dump. The readers do not need them to read, but they show that the dump is whole and that what the
// readers make of it is right: the error correction of the pages, the HMAC of the clusters and the signatures of the titles.

struct ErrorCorrectionReport
{
    std::uint32_t erasedPageCount = 0;
    std::uint32_t correctPageCount = 0;

    // The numbers of the pages that have data and no error correction bytes, and of the pages where they are wrong, in the order of the image.
    std::vector<std::uint32_t> unprotectedPages;
    std::vector<std::uint32_t> wrongPages;
};

// Checks the error correction of the first page and then of every page after `step` pages, up to the last page of the image.
// A step of 1 checks all pages.
ErrorCorrectionReport CheckErrorCorrection(Formats::NandImage& image, std::uint32_t step);

struct HmacReport
{
    std::uint32_t checkedFileCount = 0;

    // Files bigger than the limit that was asked for.
    std::uint32_t skippedFileCount = 0;

    std::uint64_t checkedClusterCount = 0;
    std::uint64_t wrongClusterCount = 0;
    bool superblockIsCorrect = false;
};

// Checks the HMAC of every cluster of every file that is not bigger than the limit, and the HMAC of the superblock.
HmacReport CheckHmacs(Formats::NandFileSystem& fileSystem, Formats::ByteView hmacKey, std::uint32_t largestFileSize);

// What the certificates of the NAND (/sys/cert.sys) say about the signatures of one title.
struct TitleSignatureReport
{
    std::uint64_t titleId;
    Formats::SignatureCheck metadata;

    // One result for each ticket in the ticket file, in the order of the file. Empty when the NAND has no ticket for the title.
    std::vector<Formats::SignatureCheck> tickets;
};

// Checks the signature of the title metadata and of every ticket of the titles on the NAND, with the certificates in /sys/cert.sys
// and the root key that is built in. Each check goes through the chain of certificates up to the root, which is slow without optimization.
std::vector<TitleSignatureReport> CheckSignaturesOfTitles(Formats::NandFileSystem& fileSystem, std::span<const std::uint64_t> titleIds);
