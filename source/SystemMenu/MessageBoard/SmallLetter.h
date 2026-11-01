#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MessageBoard/LetterPicture.h"
#include "SystemMenu/MessageBoard/MessageStore.h"
#include "SystemMenu/MessageBoard/MiiPortraits.h"

#include "Layout/LayoutArchive.h"

#include <memory>
#include <string>

namespace SystemMenu
{

// The largest place a letter of the board can have, the box in which it is dragged (0x81332BE0).
constexpr float LeftmostLetterPlace = -230;
constexpr float RightmostLetterPlace = 230;
constexpr float LowestLetterPlace = -80;
constexpr float HighestLetterPlace = 180;

// A message on the board: a small letter at its place on the day (0x813944D4). A memo is LetterS_a, a play log LetterS_c and every
// other message LetterS_b. It carries the first words of its text, the face of its Mii and a pin that shines while the message
// is new.
class SmallLetter
{
public:
    // The display, the archive and the portraits must outlive the letter. The time decides whether the pin shines.
    SmallLetter(Display& display, const Layout::LayoutArchive& boardArchive, StoredMessage storedMessage, MiiPortraits& portraits,
        std::uint64_t ticksNow);

    StoredMessage& Message();

    // The place of the letter on its day, in the units of the layout, from the middle of the day's picture.
    Formats::Vector2 PlaceOnTheDay() const;
    void MoveOnTheDay(Formats::Vector2 place);

    // The letter is drawn with the middle of its day at this place of the picture.
    void PutDayAt(Formats::Vector2 dayPlace);

    bool IsPointedAt(Formats::Vector2 place);
    bool ShowsAPicture() const;

    // The letter lands on the board, rises under a pointer and sinks again, is picked to be opened, or goes away.
    void Paste();
    void Focus();
    void Unfocus();
    void Select();
    void Leave();
    bool IsMoving() const;

    void Calculate();
    void Draw();

    // The text of a small letter: the first line of the memo or the name of the sender, cut to six characters and "..." when it is longer
    // (0x813956FC).
    static std::u16string ShortTextOf(const Formats::BoardMessage& message);

private:
    void ShowTheMii(MiiPortraits& portraits);
    void ShowThePicture(Graphics::GraphicsDevice& device);

    StoredMessage stored;
    std::unique_ptr<MenuLayout> layout;
    std::unique_ptr<LetterPicture> picture;
    Formats::Vector2 dayPlace{0, 0};
    std::size_t pasteAnimation = 0;
    std::size_t focusInAnimation = 0;
    std::size_t focusOutAnimation = 0;
    std::size_t selectAnimation = 0;
    std::size_t leaveAnimation = 0;
    std::size_t pinAnimation = 0;
};

}
