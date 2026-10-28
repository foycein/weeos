#pragma once

#include "SystemMenu/WiiSettings/PageContent.h"

#include <functional>
#include <string>
#include <vector>

namespace SystemMenu
{

// Puts the buttons, texts and pictures of a page in their places. The places are those of the pages of the original, so a page
// that is a list of choices has the same look as every other one.
class PageBuilder
{
public:
    // The page must outlive the builder.
    explicit PageBuilder(Page& pageToFill);

    void SetTitle(int tabDepth, std::u16string title);

    // The three squares at the lower right of the first pages. The square of the page that is shown is lit.
    void SetPageNumbers(int pageCount, int pageNumber);

    // A button of a page that is a list of places to go to. The slot counts from the top, from 0.
    PageButton& AddMenuItem(int slot, std::u16string text, std::function<void()> onPress);

    // One of slotCount choices, one below the other in the middle of the page. The slot counts from the top, from 0.
    PageButton& AddChoice(int slot, int slotCount, std::u16string text, bool isChosen, std::function<void()> onPress);

    // The button "Back" in the lower left corner.
    PageButton& AddBackButton(std::function<void()> onPress);

    PageButton& AddBottomLeft(std::u16string text, std::function<void()> onPress);
    PageButton& AddBottomRight(std::u16string text, std::function<void()> onPress);

    // The arrows at the sides of the first pages, which turn to the page before or the page after.
    PageButton& AddPageArrow(ButtonLook look, std::function<void()> onPress);

    // An arrow at the given place that changes a number. It presses again and again while it is held.
    PageButton& AddStepArrow(ButtonLook look, int left, int top, std::string name, std::function<void()> onPress);

    // A button that is a picture, like the televisions that a page lets the user choose from.
    PageButton& AddPictureButton(ButtonLook look, const PageRectangle& bounds, std::string name, std::function<void()> onPress);

    // Lines of text in the middle of the page, one below the other.
    void AddMessage(std::vector<std::u16string> lines);

    void AddText(const PageRectangle& bounds, TextRole role, std::vector<std::u16string> lines);
    void AddPicture(std::string imagePath, const PageRectangle& bounds);

private:
    PageButton& AddButton(ButtonLook look, const PageRectangle& bounds, std::u16string text, std::function<void()> onPress);

    Page& page;
};

}
