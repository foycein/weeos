#pragma once

#include "Channels/Manual/PageScript.h"
#include "Channels/PagePainter.h"

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Formats/Vectors.h"
#include "Text/TextLayouter.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Channels
{

// How a run of text of a page looks.
struct PageTextLook
{
    float size = 16;
    Formats::Rgba8Color color = {0, 0, 0, 255};
    bool isBold = false;
    float letterSpacing = 0;
};

enum class PagePartKind
{
    Fill,
    Picture,
    Text,
};

// One thing a laid-out page draws, in pixels of the page from its top left corner, in the order they are drawn.
struct PagePart
{
    PagePartKind kind = PagePartKind::Fill;
    PageRectangle place;

    // A fill.
    Formats::Rgba8Color color = {0, 0, 0, 0};

    // A picture: its path in the manual's files. A tiled picture is repeated at its own size from the top left corner of the place,
    // as a background is. The element name is the name or id of the <img> or of the element whose background it is, so that a script
    // can swap the picture.
    std::string path;
    std::string elementName;
    bool isTiled = false;

    // A text: one line of it, from the left top corner of the place.
    std::u16string text;
    PageTextLook look;
};

// A place of a page that does something under the pointer: a link, or an element with handlers.
struct PageLink
{
    PageRectangle place;
    std::vector<PageAction> whenPointerComes;
    std::vector<PageAction> whenPointerLeaves;

    // What A does: the handlers of onclick first, then the page of href.
    std::vector<PageAction> whenPressed;
};

// A part of the screen that shows a document of its own: a frame of a frameset, or the whole screen for a page without frames.
// Its parts and links are those from first up to, but not including, end. A frame whose content is higher than the frame scrolls.
struct PageFrameArea
{
    PageRectangle place;
    float contentHeight = 0;
    std::size_t firstPart = 0;
    std::size_t endPart = 0;
    std::size_t firstLink = 0;
    std::size_t endLink = 0;
};

struct LaidOutPage
{
    // The path of the page in the manual's files.
    std::string path;

    // How far down the page reaches.
    float height = 0;

    std::vector<PagePart> parts;
    std::vector<PageLink> links;

    // The frames of a frameset, in the order of the frameset. Empty for a page without frames.
    std::vector<PageFrameArea> frames;
};

// What the layout needs to know about the manual's files.
struct PageFiles
{
    // The bytes of a file by its path, for example "html/channel.css". Nothing when there is no such file.
    std::function<std::optional<Formats::ByteView>(std::string_view path)> readFile;

    // The size of a picture in pixels, for an <img> that does not say it. Nothing when the picture cannot be read.
    std::function<std::optional<Formats::Vector2>(std::string_view path)> pictureSize;
};

// Lays out the page at path on a screen of the given size, as the browser of the channel does for the pages of its manual: blocks,
// tables, text that breaks at spaces, pictures, backgrounds and frames (a frameset gives each frame its own part of the screen).
// The text is measured with the font. Throws a ChannelError when the page is not there.
LaidOutPage LayOutPage(const PageFiles& files, std::string_view path, const Text::TextLayouter& font, float screenWidth, float screenHeight);

// The path of a file that a page at pagePath names with relativePath: "../../img/a.png" from "html/ukv/page_01/page_01.html"
// is "html/img/a.png".
std::string ResolvePath(std::string_view pagePath, std::string_view relativePath);

// The folder of a path with its last slash, or an empty text.
std::string FolderOf(std::string_view path);

}
