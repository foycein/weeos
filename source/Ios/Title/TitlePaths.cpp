#include "Ios/Title/TitlePaths.h"

#include <charconv>
#include <format>

namespace Ios
{

namespace
{

constexpr std::size_t DigitsInNameNumber = 8;
constexpr int HexadecimalBase = 16;

// Every path here is built from numbers and constant names, so it always has the shape the file system accepts.
NandPath ParseBuiltPath(const std::string& text)
{
    return NandPath::Parse(text).Value();
}

}

std::string FormatNameNumber(std::uint32_t number)
{
    return std::format("{:08x}", number);
}

std::optional<std::uint32_t> ParseNameNumber(std::string_view name)
{
    if (name.size() != DigitsInNameNumber)
        return std::nullopt;

    std::uint32_t number = 0;
    const char* end = name.data() + name.size();
    std::from_chars_result parsed = std::from_chars(name.data(), end, number, HexadecimalBase);
    if (parsed.ec != std::errc() || parsed.ptr != end)
        return std::nullopt;

    return number;
}

NandPath TitlesPath()
{
    return ParseBuiltPath("/title");
}

NandPath TitleFamilyPath(std::uint32_t highPart)
{
    return ParseBuiltPath(std::format("/title/{}", FormatNameNumber(highPart)));
}

NandPath TitlePath(TitleId titleId)
{
    return ParseBuiltPath(std::format("/title/{}/{}", FormatNameNumber(titleId.highPart), FormatNameNumber(titleId.lowPart)));
}

NandPath TitleMetadataPath(TitleId titleId)
{
    return ParseBuiltPath(std::format("{}/content/title.tmd", TitlePath(titleId).ToString()));
}

NandPath ContentFolderPath(TitleId titleId)
{
    return ParseBuiltPath(std::format("{}/content", TitlePath(titleId).ToString()));
}

NandPath PrivateContentPath(TitleId titleId, std::uint32_t contentId)
{
    return ParseBuiltPath(std::format("{}/content/{}.app", TitlePath(titleId).ToString(), FormatNameNumber(contentId)));
}

NandPath DataFolderPath(TitleId titleId)
{
    return ParseBuiltPath(std::format("{}/data", TitlePath(titleId).ToString()));
}

NandPath TicketsPath()
{
    return ParseBuiltPath("/ticket");
}

NandPath TicketFamilyPath(std::uint32_t highPart)
{
    return ParseBuiltPath(std::format("/ticket/{}", FormatNameNumber(highPart)));
}

NandPath TicketFilePath(TitleId titleId)
{
    return ParseBuiltPath(std::format("/ticket/{}/{}.tik", FormatNameNumber(titleId.highPart), FormatNameNumber(titleId.lowPart)));
}

NandPath SharedContentFolderPath()
{
    return ParseBuiltPath("/shared1");
}

NandPath SharedContentMapPath()
{
    return ParseBuiltPath("/shared1/content.map");
}

NandPath SharedContentPath(std::string_view name)
{
    return ParseBuiltPath(std::format("/shared1/{}.app", name));
}

NandPath UserIdFilePath()
{
    return ParseBuiltPath("/sys/uid.sys");
}

NandPath CertificateFilePath()
{
    return ParseBuiltPath("/sys/cert.sys");
}

NandPath ImportFamilyPath(std::uint32_t highPart)
{
    return ParseBuiltPath(std::format("/import/{}", FormatNameNumber(highPart)));
}

NandPath ImportTitlePath(TitleId titleId)
{
    return ParseBuiltPath(std::format("/import/{}/{}", FormatNameNumber(titleId.highPart), FormatNameNumber(titleId.lowPart)));
}

NandPath ImportContentFolderPath(TitleId titleId)
{
    return ParseBuiltPath(std::format("{}/content", ImportTitlePath(titleId).ToString()));
}

NandPath ImportMetadataPath(TitleId titleId)
{
    return ParseBuiltPath(std::format("{}/content/title.tmd", ImportTitlePath(titleId).ToString()));
}

NandPath ImportContentPath(TitleId titleId, std::uint32_t contentId)
{
    return ParseBuiltPath(std::format("{}/content/{}.app", ImportTitlePath(titleId).ToString(), FormatNameNumber(contentId)));
}

NandPath TemporaryMetadataPath()
{
    return ParseBuiltPath("/tmp/title.tmd");
}

NandPath TemporaryContentPath(std::uint32_t contentId)
{
    return ParseBuiltPath(std::format("/tmp/{}.app", FormatNameNumber(contentId)));
}

NandPath TemporarySharedContentPath(std::string_view name)
{
    return ParseBuiltPath(std::format("/tmp/{}.app", name));
}

}
