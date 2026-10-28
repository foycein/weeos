#include "Layout/TextBoxDrawing.h"

#include "Layout/PaneRectangle.h"
#include "Layout/TransformMatrix.h"

#include "Text/TextStyle.h"

namespace Layout
{

namespace
{

Text::TextArea MakeTextArea(const PaneRectangle& rectangle)
{
    Text::TextArea area;
    area.left = rectangle.left;
    area.top = rectangle.top;
    area.width = rectangle.width;
    area.height = rectangle.height;
    return area;
}

// The point (x, y) of the text lands at translation + x * xAxis + y * yAxis.
Text::TextTransform MakeTextTransform(const TransformMatrix& matrix)
{
    Text::TextTransform transform;
    transform.xAxis = TransformDirection(matrix, {1.0f, 0.0f});
    transform.yAxis = TransformDirection(matrix, {0.0f, 1.0f});
    transform.translation = TransformPoint(matrix, {0.0f, 0.0f});
    return transform;
}

}

void DrawTextBox(LiveLayout& layout, const PanePlacement& placement, const TextFonts& textFonts)
{
    const Formats::Pane& pane = *placement.pane;
    const Formats::TextBox& textBox = *pane.textBox;
    if (textBox.text.empty())
        return;

    const DrawableFont& font = textFonts.Find(layout.FontNameAt(textBox.fontIndex));
    const Formats::Material& material = layout.MaterialAt(textBox.materialIndex).description;

    Text::TextStyle style = Text::MakeTextStyle(textBox, material);
    Text::TextArea area = MakeTextArea(RectangleOfPane(pane));
    Text::LaidOutText laidOutText = font.layouter.LayOut(textBox.text, style, area, placement.alpha);

    font.drawer.Draw(laidOutText, MakeTextTransform(placement.matrix));
}

}
