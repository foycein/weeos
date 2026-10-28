#pragma once

#include "SystemMenu/TitleContents.h"

#include "Formats/U8Archive.h"
#include "Layout/LayoutArchive.h"
#include "Text/MessageCatalog.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace SystemMenu
{

// The content that holds the resources of the menu: a U8 archive with the layouts, the messages, the sounds and one font.
constexpr std::uint16_t ResourceContentIndex = 1;

// The files inside the resource content of the System Menu title.
class MenuFiles
{
public:
    // Reads the whole resource content. Throws a MenuError when the title has no such content.
    explicit MenuFiles(TitleContents& titleContents);

    // A file of the resource archive, for example "message/eng/ipl_common.bmg". The bytes stay valid as long as this object lives.
    // Throws a MenuError when the archive has no such file.
    Formats::ByteView ReadFile(std::string_view path) const;

    // The layouts, animations and pictures of the file "layout/common/<bundleName>.ash", for example the bundleName "health".
    std::unique_ptr<Layout::LayoutArchive> OpenLayoutBundle(std::string_view bundleName) const;

    // The messages of the menu in English. WeeOS has only this language for now.
    Text::MessageCatalog OpenEnglishMessages() const;

    // A content that the System Menu title shares with other titles, for example the content 5 with the two small fonts.
    Formats::Bytes ReadSharedContent(std::uint16_t contentIndex) const;

private:
    TitleContents& contents;
    Formats::U8Archive resourceArchive;
};

}
