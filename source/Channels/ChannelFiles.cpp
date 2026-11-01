#include "Channels/ChannelFiles.h"

#include "Channels/ChannelError.h"

#include "Formats/Lz77.h"

#include <format>
#include <optional>

namespace Channels
{

Formats::U8Archive ReadArchiveContent(SystemMenu::TitleContents& contents, std::uint16_t contentIndex)
{
    return Formats::ReadU8Archive(contents.ReadContent(contentIndex));
}

Formats::ByteView FindFileOrFail(const Formats::U8Archive& archive, std::string_view path)
{
    std::optional<Formats::ByteView> file = archive.FindFile(path);
    if (!file)
        throw ChannelError(std::format("The archive of the channel has no file {}", path));

    return *file;
}

Formats::Bytes ReadPackedFile(const Formats::U8Archive& archive, std::string_view path)
{
    return Formats::DecompressLz77(FindFileOrFail(archive, path));
}

}
