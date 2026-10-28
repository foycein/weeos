#include "Formats/NandTitles.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/HexText.h"

#include <algorithm>
#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view TitlesPath = "/title";
constexpr std::string_view SharedContentMapPath = "/shared1/content.map";
constexpr std::size_t SharedContentNameSize = 8;
constexpr std::size_t HexDigitsPerWord = 8;
constexpr int BitsPerWord = 32;

// A content whose type has this bit is not kept in the folder of the title. Titles share it.
constexpr std::uint16_t SharedContentType = 0x8000;

std::uint32_t HighWordOf(std::uint64_t titleId)
{
    return static_cast<std::uint32_t>(titleId >> BitsPerWord);
}

std::uint32_t LowWordOf(std::uint64_t titleId)
{
    return static_cast<std::uint32_t>(titleId);
}

std::uint64_t MakeTitleId(std::uint32_t highWord, std::uint32_t lowWord)
{
    return (std::uint64_t(highWord) << BitsPerWord) | lowWord;
}

std::string MetadataPath(std::uint64_t titleId)
{
    return TitleFolderPath(titleId) + "/content/title.tmd";
}

// Gives the number that eight hexadecimal digits write, or nothing when the text is something else.
std::optional<std::uint32_t> ParseWord(std::string_view text)
{
    if (text.size() != HexDigitsPerWord)
        return std::nullopt;

    try
    {
        Bytes wordBytes = ParseHexText(text);
        ByteReader reader(wordBytes, "Title id");
        return reader.ReadUint32();
    }
    catch (const FormatError&)
    {
        return std::nullopt;
    }
}

}

std::string FormatTitleId(std::uint64_t titleId)
{
    return std::format("{:08x}-{:08x}", HighWordOf(titleId), LowWordOf(titleId));
}

std::string TitleFolderPath(std::uint64_t titleId)
{
    return std::format("{}/{:08x}/{:08x}", TitlesPath, HighWordOf(titleId), LowWordOf(titleId));
}

std::string TicketFilePath(std::uint64_t titleId)
{
    return std::format("/ticket/{:08x}/{:08x}.tik", HighWordOf(titleId), LowWordOf(titleId));
}

std::uint64_t ParseTitleId(std::string_view text)
{
    std::string_view highText = text.substr(0, HexDigitsPerWord);
    std::string_view lowText = text.substr(std::min(text.size(), HexDigitsPerWord));
    if (lowText.starts_with('-'))
        lowText.remove_prefix(1);

    std::optional<std::uint32_t> highWord = ParseWord(highText);
    std::optional<std::uint32_t> lowWord = ParseWord(lowText);
    if (!highWord || !lowWord)
        throw FormatError(std::format("Title id: \"{}\" is not two numbers of 8 hexadecimal digits like 00000001-00000002", text));

    return MakeTitleId(*highWord, *lowWord);
}

NandTitles::NandTitles(NandFileSystem& nandFileSystem)
    : fileSystem(nandFileSystem)
    , sharedContents(ReadSharedContentMap())
{
}

std::vector<std::uint64_t> NandTitles::ListTitleIds() const
{
    std::vector<std::uint64_t> titleIds;
    if (fileSystem.Find(TitlesPath) == nullptr)
        return titleIds;

    for (const NandEntry* family : fileSystem.ListDirectory(TitlesPath))
    {
        std::optional<std::uint32_t> highWord = ParseWord(family->name);
        if (!highWord)
            continue;

        for (const NandEntry* title : fileSystem.ListDirectory(family->path))
        {
            std::optional<std::uint32_t> lowWord = ParseWord(title->name);
            if (!lowWord)
                continue;

            std::uint64_t titleId = MakeTitleId(*highWord, *lowWord);
            if (fileSystem.Find(MetadataPath(titleId)) != nullptr)
                titleIds.push_back(titleId);
        }
    }
    return titleIds;
}

Bytes NandTitles::ReadMetadataBytes(std::uint64_t titleId)
{
    return fileSystem.ReadFile(MetadataPath(titleId));
}

TitleMetadata NandTitles::ReadMetadata(std::uint64_t titleId)
{
    return ReadTitleMetadata(ReadMetadataBytes(titleId));
}

std::optional<Bytes> NandTitles::ReadTicketBytes(std::uint64_t titleId)
{
    std::string ticketPath = TicketFilePath(titleId);
    if (fileSystem.Find(ticketPath) == nullptr)
        return std::nullopt;

    return fileSystem.ReadFile(ticketPath);
}

std::optional<std::string> NandTitles::FindContentPath(std::uint64_t titleId, const ContentRecord& record) const
{
    std::string contentPath;
    if (record.type & SharedContentType)
    {
        auto shared = std::find_if(sharedContents.begin(), sharedContents.end(),
            [&](const SharedContent& content) { return content.hash == record.hash; });
        if (shared == sharedContents.end())
            return std::nullopt;

        contentPath = std::format("/shared1/{}.app", shared->name);
    }
    else
    {
        contentPath = std::format("{}/content/{:08x}.app", TitleFolderPath(titleId), record.id);
    }

    if (fileSystem.Find(contentPath) == nullptr)
        return std::nullopt;

    return contentPath;
}

Bytes NandTitles::ReadContent(std::uint64_t titleId, const ContentRecord& record)
{
    std::optional<std::string> contentPath = FindContentPath(titleId, record);
    if (!contentPath)
    {
        throw FormatError(std::format("Title {}: the NAND does not have the content {:08x}",
            FormatTitleId(titleId), record.id));
    }

    Bytes content = fileSystem.ReadFile(*contentPath);
    if (content.size() != record.size)
    {
        throw FormatError(std::format("Title {}: the content {:08x} has {} bytes, but the title metadata says {}",
            FormatTitleId(titleId), record.id, content.size(), record.size));
    }

    if (ComputeSha1(content) != record.hash)
    {
        throw FormatError(std::format("Title {}: the SHA-1 hash of the content {:08x} does not match the title metadata",
            FormatTitleId(titleId), record.id));
    }

    return content;
}

std::vector<NandTitles::SharedContent> NandTitles::ReadSharedContentMap()
{
    if (fileSystem.Find(SharedContentMapPath) == nullptr)
        return {};

    Bytes mapBytes = fileSystem.ReadFile(SharedContentMapPath);
    ByteReader reader(mapBytes, "content.map");

    std::vector<SharedContent> contents;
    while (!reader.IsAtEnd())
    {
        SharedContent content;
        content.name = reader.ReadText(SharedContentNameSize);
        content.hash = reader.ReadByteArray<Sha1DigestSize>();
        contents.push_back(content);
    }
    return contents;
}

}
