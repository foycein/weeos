#pragma once

#include "SystemMenu/MenuServices.h"
#include "SystemMenu/MessageBoard/LetterPaper.h"
#include "SystemMenu/MessageBoard/MessageStore.h"
#include "SystemMenu/MessageBoard/MiiPortraits.h"

#include "Layout/LayoutArchive.h"

#include <memory>

namespace SystemMenu
{

class BottomBarScreen;

enum class LetterViewResult
{
    StillOpen,
    Closed,

    // The user threw the letter into the trash; the board deletes its record.
    Thrown,
};

// A letter of the board opened to be read (the viewer of the original, 0x81396D28 to 0x8139BC14). A memo and a play log are shown on
// the paper of my_Memo_a, every other letter on my_LetterL. The arrows of the paper and the D-pad scroll it. The bar has Back and
// Trash; Trash asks "Erase this?" first.
class LetterView
{
public:
    // The services, the archive, the portraits and the bar must outlive the view. The view starts to open at once.
    LetterView(MenuServices& menuServices, const Layout::LayoutArchive& boardArchive, const StoredMessage& message, MiiPortraits& portraits,
        BottomBarScreen& bottomBar, Formats::Vector2 placeOfTheSmallLetter);

    LetterViewResult Run();
    void Draw();

    // For the checks of the tests.
    LetterPaper& Paper();
    float ScrollPosition() const;

private:
    enum class ViewState
    {
        Opening,
        Reading,
        Scrolling,
        AskingToThrow,
        Closing,
        Throwing,
    };

    void FillThePaper(const StoredMessage& message, MiiPortraits& portraits);
    void RunScrolling();
    void RunAskingToThrow();
    void RunReading();
    void HandleTheDPad();
    void HandleThePresses();
    bool HandleArrowPress(Formats::Vector2 place);
    bool HandleBarPress(Formats::Vector2 place);
    void StartScrollingTo(float target);
    void ShowTheArrowsOfThePaper();
    void PutTheRootAtTheOpeningPlace();

    MenuServices& services;
    BottomBarScreen& bar;
    LetterPaper paper;
    MenuLayout& layout;
    Formats::Vector2 openingPlace;
    ViewState state = ViewState::Opening;
    int stepsSinceOpening = 0;

    std::size_t openAnimation = 0;
    std::size_t closeAnimation = 0;
    std::size_t arrowLoopAnimation = 0;
    std::size_t upArrowAppears = 0;
    std::size_t upArrowGoes = 0;
    std::size_t downArrowAppears = 0;
    std::size_t downArrowGoes = 0;
    bool isUpArrowShown = false;
    bool isDownArrowShown = false;

    float scroll = 0;
    float scrollSpeed = 0;
    float scrollStart = 0;
    float scrollTarget = 0;
    int scrollStep = 0;
};

}
