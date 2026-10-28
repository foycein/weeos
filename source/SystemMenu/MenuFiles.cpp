#include "SystemMenu/MenuFiles.h"

#include "SystemMenu/MenuError.h"

#include "Formats/Ash0.h"
#include "Formats/MessageFile.h"

#include <format>
#include <string>
#include <utility>

namespace SystemMenu
{

namespace
{

constexpr std::string_view EnglishMessagesPath = "message/eng/ipl_common.bmg";

}

MenuFiles::MenuFiles(TitleContents& titleContents)
    : contents(titleContents)
    , resourceArchive(Formats::ReadU8Archive(titleContents.ReadContent(ResourceContentIndex)))
{
}

Formats::Bytes MenuFiles::ReadSharedContent(std::uint16_t contentIndex) const
{
    return contents.ReadContent(contentIndex);
}

Formats::ByteView MenuFiles::ReadFile(std::string_view path) const
{
    std::optional<Formats::ByteView> file = resourceArchive.FindFile(path);
    if (!file)
        throw MenuError(std::format("The resource content of the System Menu has no file {}", path));

    return *file;
}

std::unique_ptr<Layout::LayoutArchive> MenuFiles::OpenLayoutBundle(std::string_view bundleName) const
{
    Formats::ByteView compressedBundle = ReadFile(std::format("layout/common/{}.ash", bundleName));
    Formats::U8Archive bundle = Formats::ReadU8Archive(Formats::DecompressAsh0(compressedBundle));
    return std::make_unique<Layout::LayoutArchive>(std::move(bundle));
}

Text::MessageCatalog MenuFiles::OpenEnglishMessages() const
{
    return Text::MessageCatalog(Formats::ReadMessageFile(ReadFile(EnglishMessagesPath)));
}

}
