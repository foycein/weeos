#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace SystemMenu
{

// The pages of Wii Settings are drawn in a picture of 608 by 456 points, with the origin at the top left corner and y growing downward.
constexpr int PageWidth = 608;
constexpr int PageHeight = 456;

struct PageRectangle
{
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;

    bool Contains(int x, int y) const
    {
        return x >= left && x < left + width && y >= top && y < top + height;
    }
};

// What a button looks like. The page view knows the pictures that belong to each look.
enum class ButtonLook
{
    // A white button with a line of text on a page that is a list of choices to go to (400 by 60).
    MenuItem,

    // A white button for one of several things to choose from, with a frame around it when it is chosen (394 by 70).
    ChoiceItem,

    // A button in the lower left or lower right corner (272 by 72): Back, OK, Confirm and the like.
    BottomLeft,
    BottomRight,

    // A smaller button for a list of countries (432 by 56).
    CountryItem,

    // The pictures of a television, of 4:3 (200 by 140) and of 16:9 (264 by 140), that are buttons.
    TelevisionFourByThree,
    TelevisionSixteenByNine,

    // The round buttons of the gauge of the sensitivity (32 by 32).
    MinusCircle,
    PlusCircle,

    // An arrow to the left or right to turn to another page, or one to change a number.
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown,
};

enum class TextRole
{
    // White text on the dark background: the messages of a page.
    Message,

    // Large white digits, like those of the date.
    BigNumber,

    // Dark text on a white background that the page draws itself.
    Label,
};

// The sounds that the pages play. The names are the ones of the sound archive of the menu.
enum class PageSound
{
    None,
    Decide,
    Back,
    Choose,
    Scroll,
    Error,
    Surround,
};

struct PageButton
{
    ButtonLook look = ButtonLook::MenuItem;
    PageRectangle bounds;

    // What the button says, and the name that tests use to find it (the text of a button with text, the name of an arrow).
    std::u16string text;
    std::string name;

    // A choice that is chosen has a frame around it.
    bool isChosen = false;

    // A button that is not enabled is dark and does nothing.
    bool isEnabled = true;

    // A button that is not shown does nothing and is not drawn.
    bool isShown = true;

    // A button that is held down presses again and again, as the arrows that change a number do.
    bool repeatsWhenHeld = false;

    PageSound sound = PageSound::Decide;
    std::function<void()> onPress;
};

struct PageText
{
    PageRectangle bounds;
    TextRole role = TextRole::Message;
    std::vector<std::u16string> lines;
};

// A picture of the pages' own pictures, at a place. The path is the one inside the archive of the pages, for example "FIX/COMMON/BTN/4_3_off.png".
struct PagePicture
{
    std::string imagePath;
    PageRectangle bounds;
};

// One page: the tabs at the top, the buttons, the texts and the pictures.
struct Page
{
    // How many small tabs are in front of the large one that holds the title: 0 for the first pages, 1 for the pages they lead to, and so on.
    int tabDepth = 0;
    std::u16string title;

    // The page numbers at the lower right of the first pages: 3 squares, the one for the page that is shown is lit. Nothing when 0.
    int pageCount = 0;
    int pageNumber = 0;

    std::vector<PageButton> buttons;
    std::vector<PageText> texts;
    std::vector<PagePicture> pictures;
};

}
