#include "SystemMenu/WiiSettings/PageBuilder.h"

#include <utility>

namespace SystemMenu
{

namespace
{

// A button of a list of places to go to: 400 by 60 points, one below the other every 72 points.
constexpr int MenuItemLeft = 104;
constexpr int MenuItemFirstTop = 78;
constexpr int MenuItemDistance = 72;
constexpr int MenuItemWidth = 400;
constexpr int MenuItemHeight = 60;

// A choice: 394 by 70 points, one below the other every 96 points. Fewer choices than three are moved down so that they stay in the
// middle.
constexpr int ChoiceLeft = 107;
constexpr int ChoiceFirstTopOfThree = 85;
constexpr int ChoiceDistance = 96;
constexpr int ChoiceWidth = 394;
constexpr int ChoiceHeight = 70;
constexpr int MostChoices = 3;

// The buttons in the lower corners are 272 by 72 points.
constexpr int BottomTop = 371;
constexpr int BottomLeftLeft = 28;
constexpr int BottomRightLeft = 308;
constexpr int BottomWidth = 272;
constexpr int BottomHeight = 72;

// A message lies in the middle of the page, between the tabs and the buttons below.
constexpr PageRectangle MessageBounds = {24, 80, 560, 270};

// The names keep the characters of plain ASCII and show every other one as a question mark.
constexpr char16_t FirstCharacterBeyondAscii = 0x80;

}

std::string NameOfText(std::u16string_view text)
{
    std::string name;
    for (char16_t character : text)
    {
        bool isAscii = character < FirstCharacterBeyondAscii;
        name.push_back(isAscii ? static_cast<char>(character) : '?');
    }

    return name;
}

PageBuilder::PageBuilder(Page& pageToFill)
    : page(pageToFill)
{
}

void PageBuilder::SetTitle(int tabDepth, std::u16string title)
{
    page.tabDepth = tabDepth;
    page.title = std::move(title);
}

PageButton& PageBuilder::AddMenuItem(int slot, std::u16string text, std::function<void()> onPress)
{
    PageRectangle bounds = {MenuItemLeft, MenuItemFirstTop + slot * MenuItemDistance, MenuItemWidth, MenuItemHeight};
    return AddButton(ButtonLook::MenuItem, bounds, std::move(text), std::move(onPress));
}

PageButton& PageBuilder::AddChoice(int slot, int slotCount, std::u16string text, bool isChosen, std::function<void()> onPress)
{
    int firstTop = ChoiceFirstTopOfThree + (MostChoices - slotCount) * ChoiceDistance / 2;
    PageRectangle bounds = {ChoiceLeft, firstTop + slot * ChoiceDistance, ChoiceWidth, ChoiceHeight};

    PageButton& button = AddButton(ButtonLook::ChoiceItem, bounds, std::move(text), std::move(onPress));
    button.isChosen = isChosen;
    button.sound = PageSound::Choose;
    return button;
}

PageButton& PageBuilder::AddBackButton(std::function<void()> onPress)
{
    PageButton& button = AddBottomLeft(u"Back", std::move(onPress));
    button.sound = PageSound::Back;
    return button;
}

PageButton& PageBuilder::AddBottomLeft(std::u16string text, std::function<void()> onPress)
{
    PageRectangle bounds = {BottomLeftLeft, BottomTop, BottomWidth, BottomHeight};
    return AddButton(ButtonLook::BottomLeft, bounds, std::move(text), std::move(onPress));
}

PageButton& PageBuilder::AddBottomRight(std::u16string text, std::function<void()> onPress)
{
    PageRectangle bounds = {BottomRightLeft, BottomTop, BottomWidth, BottomHeight};
    return AddButton(ButtonLook::BottomRight, bounds, std::move(text), std::move(onPress));
}

PageButton& PageBuilder::AddPictureButton(ButtonLook look, const PageRectangle& bounds, std::string name, std::function<void()> onPress)
{
    PageButton& button = AddButton(look, bounds, u"", std::move(onPress));
    button.name = std::move(name);
    button.sound = PageSound::Choose;
    return button;
}

void PageBuilder::AddMessage(std::vector<std::u16string> lines)
{
    AddText(MessageBounds, TextRole::Message, std::move(lines));
}

void PageBuilder::AddText(const PageRectangle& bounds, TextRole role, std::vector<std::u16string> lines)
{
    page.texts.push_back({bounds, role, std::move(lines)});
}

void PageBuilder::AddPicture(std::string imagePath, const PageRectangle& bounds)
{
    page.pictures.push_back({std::move(imagePath), bounds});
}

PageButton& PageBuilder::AddButton(ButtonLook look, const PageRectangle& bounds, std::u16string text, std::function<void()> onPress)
{
    PageButton button;
    button.look = look;
    button.bounds = bounds;
    button.text = std::move(text);
    button.name = NameOfText(button.text);
    button.onPress = std::move(onPress);

    page.buttons.push_back(std::move(button));
    return page.buttons.back();
}

}
