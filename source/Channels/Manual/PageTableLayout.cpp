#include "Channels/Manual/PageLayouter.h"

#include <algorithm>
#include <numeric>

namespace Channels
{

namespace
{

// HTML's defaults for a table that does not set them.
constexpr float DefaultCellSpacing = 2;
constexpr float DefaultCellPadding = 1;

enum class VerticalPlace
{
    Top,
    Middle,
    Bottom,
};

// A cell sits in the middle of its row unless the cell or its row says otherwise.
VerticalPlace VerticalPlaceOf(const HtmlNode& cell, const HtmlNode& row)
{
    std::string valign = cell.HasAttribute("valign") ? cell.Attribute("valign") : row.Attribute("valign");
    if (valign == "top")
        return VerticalPlace::Top;
    if (valign == "bottom")
        return VerticalPlace::Bottom;

    return VerticalPlace::Middle;
}

float Sum(const std::vector<float>& numbers, std::size_t first, std::size_t count)
{
    auto start = numbers.begin() + static_cast<std::ptrdiff_t>(first);
    return std::accumulate(start, start + static_cast<std::ptrdiff_t>(count), 0.0f);
}

// Gives the room that is left to the columns that ask for no width, as wide as their content wants to be. When every column asks
// for a width, they all grow in the measure of their widths.
void ShareExtraRoom(std::vector<float>& widths, const ColumnMeasures& measures, float extra)
{
    std::vector<float> weights(widths.size(), 0);
    for (std::size_t column = 0; column < widths.size(); ++column)
    {
        if (measures.asked[column] == 0)
            weights[column] = std::max(measures.widest[column], 1.0f);
    }

    float totalWeight = Sum(weights, 0, weights.size());
    if (totalWeight == 0)
    {
        weights = widths;
        totalWeight = Sum(weights, 0, weights.size());
    }
    if (totalWeight == 0)
        return;

    for (std::size_t column = 0; column < widths.size(); ++column)
        widths[column] += extra * weights[column] / totalWeight;
}

// Cells that ask for more than the width of their table give way, down to their content: the pages ask for a cell of 1019 pixels in a
// table as wide as the screen and still show their content in the middle of the screen (CHN-13).
void NarrowAskedColumns(std::vector<float>& widths, const ColumnMeasures& measures, float excess)
{
    float give = 0;
    for (std::size_t column = 0; column < widths.size(); ++column)
        give += widths[column] - measures.narrowest[column];

    if (give <= 0)
        return;

    float share = std::min(excess / give, 1.0f);
    for (std::size_t column = 0; column < widths.size(); ++column)
        widths[column] -= (widths[column] - measures.narrowest[column]) * share;
}

}

TableLayoutValues TableValuesOf(const HtmlNode& table)
{
    TableLayoutValues values;
    values.spacing = table.HasAttribute("cellspacing") ? ReadLength(table.Attribute("cellspacing"), 0).value_or(PageLength{}).value : DefaultCellSpacing;
    values.padding = table.HasAttribute("cellpadding") ? ReadLength(table.Attribute("cellpadding"), 0).value_or(PageLength{}).value : DefaultCellPadding;
    values.rows = RowsOf(table);
    for (const TableRow& row : values.rows)
    {
        for (const TableCell& cell : row.cells)
            values.columnCount = std::max(values.columnCount, cell.column + cell.span);
    }
    return values;
}

std::vector<TableRow> RowsOf(const HtmlNode& table)
{
    std::vector<TableRow> rows;
    for (const HtmlNode& child : table.children)
    {
        bool isRowGroup = child.tag == "tbody" || child.tag == "thead" || child.tag == "tfoot";
        if (isRowGroup)
        {
            std::vector<TableRow> groupRows = RowsOf(child);
            rows.insert(rows.end(), groupRows.begin(), groupRows.end());
            continue;
        }

        if (child.tag != "tr")
            continue;

        TableRow row;
        row.element = &child;
        std::size_t column = 0;
        for (const HtmlNode& cell : child.children)
        {
            if (cell.tag != "td" && cell.tag != "th")
                continue;

            std::size_t span = std::max<std::size_t>(1, static_cast<std::size_t>(ReadLength(cell.Attribute("colspan"), 0).value_or(PageLength{1, false}).value));
            row.cells.push_back({&cell, column, span});
            column += span;
        }
        rows.push_back(row);
    }
    return rows;
}

std::vector<float> PageLayouter::ColumnWidths(const HtmlNode& table, const BoxStyle& tableStyle, const TableLayoutValues& values, float room) const
{
    std::optional<PageLength> askedTableWidth = ReadLength(table.Attribute("width"), 0);
    if (tableStyle.width)
        askedTableWidth = tableStyle.width;

    float spacingRoom = values.spacing * static_cast<float>(values.columnCount + 1);
    std::optional<float> tableWidth;
    if (askedTableWidth)
        tableWidth = askedTableWidth->Of(room);

    ColumnMeasures measures = MeasureColumns(values, tableStyle, tableWidth.value_or(room) - spacingRoom);
    std::vector<float> widths(values.columnCount, 0);
    for (std::size_t column = 0; column < widths.size(); ++column)
        widths[column] = std::max(measures.asked[column], measures.narrowest[column]);

    if (tableWidth)
    {
        float extra = *tableWidth - spacingRoom - Sum(widths, 0, widths.size());
        if (extra > 0)
            ShareExtraRoom(widths, measures, extra);
        else
            NarrowAskedColumns(widths, measures, -extra);
        return widths;
    }

    // Without a width the table is as wide as its content wants, but not wider than the room.
    for (std::size_t column = 0; column < widths.size(); ++column)
    {
        if (measures.asked[column] == 0)
            widths[column] = measures.widest[column];
    }

    float overflow = Sum(widths, 0, widths.size()) + spacingRoom - room;
    for (std::size_t column = 0; column < widths.size() && overflow > 0; ++column)
    {
        if (measures.asked[column] != 0)
            continue;

        float shrink = std::min(overflow, widths[column] - measures.narrowest[column]);
        widths[column] -= shrink;
        overflow -= shrink;
    }
    return widths;
}

float PageLayouter::LayOutTable(const HtmlNode& table, const BoxStyle& parentStyle, float left, float top, float width)
{
    BoxStyle tableStyle = StyleOf(table, parentStyle);
    TableLayoutValues values = TableValuesOf(table);
    std::vector<float> columns = ColumnWidths(table, tableStyle, values, width);
    float tableWidth = Sum(columns, 0, columns.size()) + values.spacing * static_cast<float>(columns.size() + 1);

    float tableLeft = left;
    std::string alignment = table.Attribute("align");
    if (alignment == "center" || (alignment.empty() && parentStyle.textAlignment == TextAlignment::Center))
        tableLeft = left + (width - tableWidth) / 2;
    else if (alignment == "right")
        tableLeft = left + width - tableWidth;

    // First every cell is laid out aside, to learn the height of the rows.
    std::vector<float> rowHeights;
    std::vector<bool> rowIsAsked;
    for (const TableRow& row : values.rows)
    {
        BoxStyle rowStyle = StyleOf(*row.element, tableStyle);
        float rowHeight = ReadLength(row.element->Attribute("height"), 0).value_or(PageLength{}).value;
        bool isAsked = rowHeight > 0;
        for (const TableCell& cell : row.cells)
        {
            BoxStyle cellStyle = StyleOf(*cell.element, rowStyle);
            float cellWidth = Sum(columns, cell.column, cell.span) + values.spacing * static_cast<float>(cell.span - 1);
            float contentWidth = cellWidth - 2 * values.padding - cellStyle.paddingLeft - cellStyle.paddingRight;
            float contentHeight = MeasureHeight([&] { return LayOutChildren(*cell.element, cellStyle, 0, 0, contentWidth); });
            float askedHeight = ReadLength(cell.element->Attribute("height"), 0).value_or(PageLength{}).value;
            if (cellStyle.height)
                askedHeight = std::max(askedHeight, cellStyle.height->value);

            isAsked = isAsked || askedHeight > 0;
            rowHeight = std::max({rowHeight, askedHeight, contentHeight + 2 * values.padding + cellStyle.paddingTop + cellStyle.paddingBottom});
        }
        rowHeights.push_back(rowHeight);
        rowIsAsked.push_back(isAsked);
    }

    // A table that must be higher than its rows gives the rest to the rows that ask for no height (or to the last row).
    float rowsHeight = Sum(rowHeights, 0, rowHeights.size()) + values.spacing * static_cast<float>(rowHeights.size() + 1);
    float askedTableHeight = ReadLength(table.Attribute("height"), 0).value_or(PageLength{}).value;
    if (tableStyle.height)
        askedTableHeight = std::max(askedTableHeight, tableStyle.height->value);
    float extraHeight = askedTableHeight - rowsHeight;
    if (extraHeight > 0 && !rowHeights.empty())
    {
        std::size_t freeRows = static_cast<std::size_t>(std::count(rowIsAsked.begin(), rowIsAsked.end(), false));
        for (std::size_t row = 0; row < rowHeights.size(); ++row)
        {
            bool getsShare = freeRows == 0 ? row + 1 == rowHeights.size() : !rowIsAsked[row];
            if (getsShare)
                rowHeights[row] += extraHeight / static_cast<float>(std::max<std::size_t>(freeRows, 1));
        }
        rowsHeight = askedTableHeight;
    }

    // Then every cell is laid out in its place.
    std::size_t tableBackground = output->parts.size();
    float y = top + values.spacing;
    for (std::size_t rowIndex = 0; rowIndex < values.rows.size(); ++rowIndex)
    {
        const TableRow& row = values.rows[rowIndex];
        BoxStyle rowStyle = StyleOf(*row.element, tableStyle);
        std::size_t rowBackground = output->parts.size();
        for (const TableCell& cell : row.cells)
        {
            BoxStyle cellStyle = StyleOf(*cell.element, rowStyle);
            float cellLeft = tableLeft + values.spacing + Sum(columns, 0, cell.column) + values.spacing * static_cast<float>(cell.column);
            float cellWidth = Sum(columns, cell.column, cell.span) + values.spacing * static_cast<float>(cell.span - 1);
            float contentLeft = cellLeft + values.padding + cellStyle.paddingLeft;
            float contentWidth = cellWidth - 2 * values.padding - cellStyle.paddingLeft - cellStyle.paddingRight;
            float contentHeight = MeasureHeight([&] { return LayOutChildren(*cell.element, cellStyle, 0, 0, contentWidth); });

            float freeHeight = rowHeights[rowIndex] - contentHeight - 2 * values.padding - cellStyle.paddingTop - cellStyle.paddingBottom;
            float contentTop = y + values.padding + cellStyle.paddingTop;
            VerticalPlace place = VerticalPlaceOf(*cell.element, *row.element);
            if (place == VerticalPlace::Middle)
                contentTop += freeHeight / 2;
            else if (place == VerticalPlace::Bottom)
                contentTop += freeHeight;

            std::size_t cellBackground = output->parts.size();
            LayOutChildren(*cell.element, cellStyle, contentLeft, contentTop, contentWidth);
            DrawBackground(cellBackground, *cell.element, cellStyle, {cellLeft, y, cellWidth, rowHeights[rowIndex]});
        }

        float rowWidth = tableWidth - 2 * values.spacing;
        DrawBackground(rowBackground, *row.element, rowStyle, {tableLeft + values.spacing, y, rowWidth, rowHeights[rowIndex]});
        y += rowHeights[rowIndex] + values.spacing;
    }

    DrawBackground(tableBackground, table, tableStyle, {tableLeft, top, tableWidth, rowsHeight});
    return rowsHeight;
}

}
