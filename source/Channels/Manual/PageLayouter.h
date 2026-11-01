#pragma once

#include "Channels/Manual/HtmlDocument.h"
#include "Channels/Manual/PageLayout.h"
#include "Channels/Manual/PageScript.h"
#include "Channels/Manual/PageStyleSheet.h"
#include "Channels/Manual/PageTables.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace Channels
{

enum class TextAlignment
{
    Left,
    Center,
    Right,
};

// A length of CSS or of an attribute: pixels, or a part of the room there is.
struct PageLength
{
    float value = 0;
    bool isPercentage = false;

    float Of(float room) const;
};

// What an element looks like after the style rules, its attributes and the defaults of the browser. The first group is inherited by
// the elements inside it, the rest belongs to the element alone.
struct BoxStyle
{
    PageTextLook look;
    TextAlignment textAlignment = TextAlignment::Left;
    float textIndent = 0;

    float paddingTop = 0;
    float paddingRight = 0;
    float paddingBottom = 0;
    float paddingLeft = 0;
    float marginTop = 0;
    float marginBottom = 0;
    std::optional<PageLength> width;
    std::optional<PageLength> height;
    std::string backgroundPath;
    std::optional<Formats::Rgba8Color> backgroundColor;

    BoxStyle InheritedPart() const;
};

// The narrowest a piece of a page can be (its longest word or picture) and how wide it is when nothing breaks.
struct ContentWidths
{
    float narrowest = 0;
    float widest = 0;
};

// Lays out one document of a page. The parts are written into the page given to it; a frameset lays out each frame with a layouter
// of its own.
class PageLayouter
{
public:
    PageLayouter(const PageFiles& files, std::string_view path, const Text::TextLayouter& font, LaidOutPage& output);

    // Lays out the document in the rectangle and gives how far down it reaches.
    float LayOut(const PageRectangle& screen);

private:
    // One word, space, picture or line break of a line of text, and the link it belongs to.
    struct InlineItem
    {
        enum class Kind
        {
            Word,
            Space,
            Picture,
            LineBreak,
        };

        Kind kind = Kind::Word;
        float width = 0;
        float height = 0;
        std::u16string text;
        PageTextLook look;
        std::string path;
        std::string elementName;
        bool isCenteredInLine = false;

        // A picture with align="left" or align="right" leaves the line and stands at that side; the lines flow around it.
        std::optional<TextAlignment> floatSide;
        float verticalSpace = 0;

        // The links that the item is part of: an <img> with handlers of its own inside a link is part of both.
        std::vector<std::size_t> links;
    };

    // Reading the document (PageLayouter.cpp).
    void ReadHead(const HtmlNode& node);
    float LayOutFrameset(const HtmlNode& frameset, const PageRectangle& screen);

    // Styles (PageStyle.cpp).
    BoxStyle StyleOf(const HtmlNode& element, const BoxStyle& parentStyle) const;
    void ApplyAttributes(const HtmlNode& element, BoxStyle& style) const;
    void ApplyDeclaration(const StyleDeclaration& declaration, BoxStyle& style) const;
    float TextLineHeight(const PageTextLook& look) const;
    float TextWidth(std::u16string_view text, const PageTextLook& look) const;

    // Blocks and lines (PageLayouter.cpp).
    float LayOutChildren(const HtmlNode& node, const BoxStyle& style, float left, float top, float width);
    float LayOutBlock(const HtmlNode& element, const BoxStyle& parentStyle, float left, float top, float width);
    void CollectInlineItems(const HtmlNode& node, const BoxStyle& style, std::vector<std::size_t> links, std::vector<InlineItem>& items);
    void AddWords(std::u16string_view text, const PageTextLook& look, const std::vector<std::size_t>& links, std::vector<InlineItem>& items) const;
    InlineItem PictureItem(const HtmlNode& image, const std::vector<std::size_t>& links) const;
    // A picture that stands at a side of the lines, and where it ends.
    struct FloatingPicture
    {
        TextAlignment side;
        float width;
        float bottom;
    };

    float LayOutLines(const std::vector<InlineItem>& items, const BoxStyle& style, float left, float top, float width);
    void PlaceFloatingPicture(const InlineItem& item, float left, float top, float width, std::vector<FloatingPicture>& floats);
    float PlaceLine(const std::vector<InlineItem>& line, const BoxStyle& style, float left, float top, float width, bool isFirstLine);
    void DrawBackground(std::size_t partIndex, const HtmlNode& element, const BoxStyle& style, const PageRectangle& box);
    std::optional<std::size_t> AddLink(const HtmlNode& element);
    void GrowLink(std::size_t link, const PageRectangle& place);

    // Tables (PageTableLayout.cpp).
    float LayOutTable(const HtmlNode& table, const BoxStyle& parentStyle, float left, float top, float width);
    std::vector<float> ColumnWidths(const HtmlNode& table, const BoxStyle& tableStyle, const TableLayoutValues& values, float room) const;
    std::vector<RowHeight> MeasureRowHeights(const TableLayoutValues& values, const BoxStyle& tableStyle, const std::vector<float>& columns);
    void PlaceTableCell(const TableCell& cell, const HtmlNode& row, const BoxStyle& rowStyle, float cellPadding, const PageRectangle& cellBox);

    // Widths before layout (PageMeasure.cpp).
    ContentWidths MeasureWidths(const HtmlNode& node, const BoxStyle& style) const;
    ContentWidths MeasureTableWidths(const HtmlNode& table, const BoxStyle& style) const;
    ColumnMeasures MeasureColumns(const TableLayoutValues& values, const BoxStyle& tableStyle, std::optional<float> percentageRoom) const;
    std::optional<Formats::Vector2> PictureSizeOf(const HtmlNode& image) const;

    // Lays out with the parts going into a scratch page, to learn the height.
    template <typename LayOutFunction>
    float MeasureHeight(LayOutFunction layOut);

    const PageFiles& files;
    std::string path;
    std::string folder;
    const Text::TextLayouter& font;
    LaidOutPage* output;
    HtmlNode document;
    PageStyleSheet styleSheet;
    PageScript script;
};

template <typename LayOutFunction>
float PageLayouter::MeasureHeight(LayOutFunction layOut)
{
    LaidOutPage scratch;
    LaidOutPage* realOutput = output;
    output = &scratch;
    float height = layOut();
    output = realOutput;
    return height;
}

// Values of CSS and of attributes (PageStyle.cpp).
std::optional<PageLength> ReadLength(std::string_view text, float fontSize);

// The number of a length without its unit, or 0 when the text is no length.
float ReadLengthValue(std::string_view text, float fontSize);
std::optional<Formats::Rgba8Color> ReadColor(std::string_view text);

// The elements that start a block of their own; everything else flows in lines.
bool IsBlockElement(std::string_view tag);

// Elements that are never drawn.
bool IsHiddenElement(std::string_view tag);

}
