#pragma once

#include "SystemMenu/MenuLayout.h"

#include "Layout/LayoutArchive.h"

#include <memory>
#include <string>

namespace SystemMenu
{

// The paper of a memo (my_Memo_a) or of a letter (my_LetterL). It grows with its text: the header, one piece of the body for every line
// of the text, and the footer below the last line (0x813989F0). The whole paper moves up and down with N_Memo when it is scrolled.
class LetterPaper
{
public:
    // The display and the archive must outlive the paper.
    LetterPaper(Display& display, const Layout::LayoutArchive& archive, const std::string& layoutName);

    MenuLayout& PaperLayout();

    // Counts the pieces of the body that the text of T_Letter needs. Without fonts the paper has one.
    void MeasureTheText();
    int LineCount() const;

    // How far the paper can be scrolled: the paper and 160 units around it, less the height of the picture (0x8139A36C).
    float LongestScroll() const;

    // N_Memo moves up by the scroll.
    void ScrollTo(float scroll);

    void Draw();

private:
    void DrawPart(std::string_view partName, float offsetY);

    Display& display;
    std::unique_ptr<MenuLayout> layout;
    int lineCount = 1;
    float lineHeight = 0;
    float scroll = 0;
};

}
