#include "SystemMenu/MessageBoard/LetterPaper.h"

#include "SystemMenu/WiiMenu/LayoutParts.h"

#include "Layout/LiveLayout.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace SystemMenu
{

namespace
{

constexpr std::string_view PaperRoot = "N_Memo";
constexpr std::string_view HeaderPart = "N_Header";
constexpr std::string_view BodyPart = "N_Body";
constexpr std::string_view FooterPart = "N_Footer";
constexpr std::string_view BodyText = "T_Letter";

constexpr float SpaceAroundThePaper = 160;
constexpr float PictureHeight = 456;

// A part of the paper is drawn only while it lies within 500 units of the middle of the picture.
constexpr float DrawnDistance = 500;

}

LetterPaper::LetterPaper(Display& menuDisplay, const Layout::LayoutArchive& archive, const std::string& layoutName)
    : display(menuDisplay)
    , layout(std::make_unique<MenuLayout>(menuDisplay, archive, layoutName))
{
    lineHeight = layout->PaneNamed(BodyPart).height;
}

MenuLayout& LetterPaper::PaperLayout()
{
    return *layout;
}

void LetterPaper::MeasureTheText()
{
    lineCount = 1;
    if (display.TextFonts() == nullptr || lineHeight <= 0)
        return;

    Text::LaidOutText text = layout->LayOutText(BodyText);
    lineCount = std::max(1, static_cast<int>(std::ceil(text.block.height / lineHeight)));
}

int LetterPaper::LineCount() const
{
    return lineCount;
}

float LetterPaper::LongestScroll() const
{
    float paperHeight = layout->PaneNamed(HeaderPart).height + lineCount * lineHeight + layout->PaneNamed(FooterPart).height;
    return std::max(0.0f, SpaceAroundThePaper + paperHeight - PictureHeight);
}

void LetterPaper::ScrollTo(float newScroll)
{
    scroll = newScroll;
    layout->PaneNamed(PaperRoot).translation.y = scroll;
}

void LetterPaper::Draw()
{
    if (scroll < DrawnDistance)
        DrawPart(HeaderPart, 0);

    for (int line = 0; line < lineCount; ++line)
    {
        float lineY = -line * lineHeight;
        if (std::abs(scroll + lineY) < DrawnDistance)
            DrawPart(BodyPart, lineY);
    }

    float footerY = -(lineCount - 1) * lineHeight;
    if (scroll + footerY > -DrawnDistance)
        DrawPart(FooterPart, footerY);

    std::vector<Formats::Pane*> parts;
    for (std::string_view partName : {HeaderPart, BodyPart, FooterPart})
    {
        std::vector<Formats::Pane*> panes = Layout::PaneWithDescendants(layout->PaneNamed(partName));
        parts.insert(parts.end(), panes.begin(), panes.end());
    }
    auto isOutsideTheParts = [&](const Formats::Pane& pane) { return std::find(parts.begin(), parts.end(), &pane) == parts.end(); };
    DrawPanes(*layout, display, PlacedPanes(*layout), isOutsideTheParts);
}

void LetterPaper::DrawPart(std::string_view partName, float offsetY)
{
    Formats::Pane& part = layout->PaneNamed(partName);
    float restingY = part.translation.y;
    part.translation.y = restingY + offsetY;

    std::vector<Formats::Pane*> panes = Layout::PaneWithDescendants(part);
    auto isInThePart = [&](const Formats::Pane& pane) { return std::find(panes.begin(), panes.end(), &pane) != panes.end(); };
    DrawPanes(*layout, display, PlacedPanes(*layout), isInThePart);
    part.translation.y = restingY;
}

}
