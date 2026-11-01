#pragma once

#include "SystemMenu/TitleContents.h"

#include "Formats/Bytes.h"
#include "Formats/U8Archive.h"

#include <cstdint>
#include <string_view>

namespace Channels
{

// A content of the channel's title that is a U8 archive. The contents throw their own error when the title has no such content.
Formats::U8Archive ReadArchiveContent(SystemMenu::TitleContents& contents, std::uint16_t contentIndex);

// A file of an archive. The bytes stay valid as long as the archive. Throws a ChannelError when the archive has no such file.
Formats::ByteView FindFileOrFail(const Formats::U8Archive& archive, std::string_view path);

// A file of an archive that is packed with LZ77, as the files named "*.LZ" of the channels are, unpacked.
Formats::Bytes ReadPackedFile(const Formats::U8Archive& archive, std::string_view path);

}
