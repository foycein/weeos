#pragma once

#include "Channels/Manual/HtmlDocument.h"

#include <cstddef>
#include <vector>

namespace Channels
{

// A cell of a table: the first column it covers and how many columns it covers (colspan).
struct TableCell
{
    const HtmlNode* element;
    std::size_t column;
    std::size_t span;
};

struct TableRow
{
    const HtmlNode* element;
    std::vector<TableCell> cells;
};

// What a table says about itself before anything is laid out.
struct TableLayoutValues
{
    // The room between the cells and around them, and the room inside every cell around its content.
    float spacing = 0;
    float padding = 0;

    std::vector<TableRow> rows;
    std::size_t columnCount = 0;
};

// For every column: the width its cells ask for (0 when none asks), the narrowest its content can be, and how wide its content wants
// to be. The cell padding is included.
struct ColumnMeasures
{
    std::vector<float> asked;
    std::vector<float> narrowest;
    std::vector<float> widest;
};

TableLayoutValues TableValuesOf(const HtmlNode& table);

// The rows of the table, also those inside <tbody>, <thead> and <tfoot>.
std::vector<TableRow> RowsOf(const HtmlNode& table);

}
