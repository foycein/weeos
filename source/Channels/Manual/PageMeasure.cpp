#include "Channels/Manual/PageLayouter.h"

#include <algorithm>

namespace Channels
{

namespace
{

bool IsCollapsibleSpace(char16_t character)
{
    return character == u' ' || character == u'\t' || character == u'\n' || character == u'\r';
}

std::vector<std::u16string> WordsOf(std::u16string_view text)
{
    std::vector<std::u16string> words(1);
    for (char16_t character : text)
    {
        if (!IsCollapsibleSpace(character))
            words.back() += character;
        else if (!words.back().empty())
            words.emplace_back();
    }

    if (words.back().empty())
        words.pop_back();

    return words;
}

ContentWidths Widest(const ContentWidths& first, const ContentWidths& second)
{
    return {std::max(first.narrowest, second.narrowest), std::max(first.widest, second.widest)};
}

}

std::optional<Formats::Vector2> PageLayouter::PictureSizeOf(const HtmlNode& image) const
{
    std::optional<PageLength> width = ReadLength(image.Attribute("width"), 0);
    std::optional<PageLength> height = ReadLength(image.Attribute("height"), 0);
    if (width && height)
        return Formats::Vector2{width->value, height->value};

    std::optional<Formats::Vector2> ownSize = files.pictureSize(ResolvePath(path, image.Attribute("src")));
    if (!ownSize)
        return std::nullopt;

    // A picture that gives only one of its sides keeps its shape.
    if (width && ownSize->x > 0)
        return Formats::Vector2{width->value, ownSize->y * width->value / ownSize->x};
    if (height && ownSize->y > 0)
        return Formats::Vector2{ownSize->x * height->value / ownSize->y, height->value};

    return ownSize;
}

// How narrow the content can get and how wide it is on lines that never break: what a table needs to share its width among its columns.
ContentWidths PageLayouter::MeasureWidths(const HtmlNode& node, const BoxStyle& style) const
{
    if (node.IsText())
    {
        ContentWidths widths;
        std::u16string line;
        for (const std::u16string& word : WordsOf(node.text))
        {
            widths.narrowest = std::max(widths.narrowest, TextWidth(word, style.look));
            line += line.empty() ? word : u" " + word;
        }
        widths.widest = TextWidth(line, style.look);
        return widths;
    }

    if (IsHiddenElement(node.tag))
        return {};

    if (node.tag == "img")
    {
        float width = PictureSizeOf(node).value_or(Formats::Vector2{0, 0}).x;
        return {width, width};
    }

    if (node.tag == "table")
        return MeasureTableWidths(node, style);

    BoxStyle ownStyle = StyleOf(node, style);
    float sides = ownStyle.paddingLeft + ownStyle.paddingRight;
    if (ownStyle.width && !ownStyle.width->isPercentage)
        return {ownStyle.width->value + sides, ownStyle.width->value + sides};

    // Inline children stand side by side on one line until a line break or a block; blocks stand alone.
    ContentWidths widths;
    float lineWidth = 0;
    for (const HtmlNode& child : node.children)
    {
        bool endsLine = child.tag == "br" || (!child.IsText() && IsBlockElement(child.tag));
        ContentWidths childWidths = MeasureWidths(child, ownStyle);
        if (endsLine)
        {
            widths = Widest(widths, childWidths);
            lineWidth = 0;
            continue;
        }

        lineWidth += childWidths.widest;
        widths.narrowest = std::max(widths.narrowest, childWidths.narrowest);
        widths.widest = std::max(widths.widest, lineWidth);
    }

    return {widths.narrowest + sides, widths.widest + sides};
}

ContentWidths PageLayouter::MeasureTableWidths(const HtmlNode& table, const BoxStyle& style) const
{
    BoxStyle tableStyle = StyleOf(table, style);
    TableLayoutValues values = TableValuesOf(table);
    ColumnMeasures measures = MeasureColumns(values, tableStyle, std::nullopt);

    float spacingRoom = values.spacing * static_cast<float>(values.columnCount + 1);
    ContentWidths widths = {spacingRoom, spacingRoom};
    for (std::size_t column = 0; column < values.columnCount; ++column)
    {
        widths.narrowest += std::max(measures.asked[column], measures.narrowest[column]);
        widths.widest += std::max(measures.asked[column], measures.widest[column]);
    }

    std::optional<PageLength> askedWidth = ReadLength(table.Attribute("width"), 0);
    if (askedWidth && !askedWidth->isPercentage)
    {
        widths.narrowest = std::max(widths.narrowest, askedWidth->value);
        widths.widest = std::max(widths.widest, askedWidth->value);
    }
    return widths;
}

// A width in percent counts from the room of the table; while the table itself is only measured it has no room yet, and such
// widths count as nothing.
ColumnMeasures PageLayouter::MeasureColumns(const TableLayoutValues& values, const BoxStyle& tableStyle, std::optional<float> percentageRoom) const
{
    ColumnMeasures measures;
    measures.asked.assign(values.columnCount, 0);
    measures.narrowest.assign(values.columnCount, 0);
    measures.widest.assign(values.columnCount, 0);

    for (const TableRow& row : values.rows)
    {
        BoxStyle rowStyle = StyleOf(*row.element, tableStyle);
        for (const TableCell& cell : row.cells)
        {
            ContentWidths content = MeasureWidths(*cell.element, rowStyle);

            float asked = 0;
            std::optional<PageLength> askedWidth = ReadLength(cell.element->Attribute("width"), 0);
            if (askedWidth && (!askedWidth->isPercentage || percentageRoom))
                asked = askedWidth->Of(percentageRoom.value_or(0));

            if (cell.span == 1)
            {
                measures.asked[cell.column] = std::max(measures.asked[cell.column], asked);
                measures.narrowest[cell.column] = std::max(measures.narrowest[cell.column], content.narrowest + 2 * values.padding);
                measures.widest[cell.column] = std::max(measures.widest[cell.column], content.widest + 2 * values.padding);
                continue;
            }

            // A cell over several columns that is narrower than its content widens each of them by the same part.
            float spanned = 0;
            for (std::size_t column = cell.column; column < cell.column + cell.span; ++column)
                spanned += std::max(measures.asked[column], measures.narrowest[column]);

            float missing = content.narrowest + 2 * values.padding - spanned;
            for (std::size_t column = cell.column; column < cell.column + cell.span && missing > 0; ++column)
                measures.narrowest[column] += missing / static_cast<float>(cell.span);
        }
    }
    return measures;
}

}
