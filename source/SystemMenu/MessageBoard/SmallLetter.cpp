#include "SystemMenu/MessageBoard/SmallLetter.h"

#include "SystemMenu/WiiMenu/LayoutParts.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

constexpr std::string_view LetterButton = "B_Letter";
constexpr std::string_view LetterText = "T_Letter";
constexpr std::string_view MiiPicture = "Nigaoe";
constexpr std::string_view PictureFrame = "N_Pic";
constexpr std::string_view PinGroup = "G_New";

// A letter of the board shows six characters of its text, and "..." when the line is longer (0x813956FC).
constexpr std::size_t ShortTextLength = 6;
constexpr std::u16string_view Ellipsis = u"...";

// A message shows the shining pin for six hours after it came onto the board (0x813944D4).
constexpr std::uint64_t NewForSeconds = 6 * 60 * 60;

std::string LayoutNameOf(Formats::BoardMessageKind kind)
{
    if (kind == Formats::BoardMessageKind::Memo)
        return "LetterS_a";
    if (kind == Formats::BoardMessageKind::PlayLog)
        return "LetterS_c";
    return "LetterS_b";
}

std::string AnimationName(const std::string& layoutName, std::string_view animation)
{
    return layoutName + "_" + std::string(animation);
}

}

SmallLetter::SmallLetter(Display& display, const Layout::LayoutArchive& boardArchive, StoredMessage storedMessage, MiiPortraits& portraits,
    std::uint64_t ticksNow)
    : stored(std::move(storedMessage))
{
    std::string layoutName = LayoutNameOf(stored.message.kind);
    layout = std::make_unique<MenuLayout>(display, boardArchive, layoutName);
    pasteAnimation = layout->BindAnimationToAllPanes(AnimationName(layoutName, "PasteLetter"));
    focusInAnimation = layout->BindAnimationToAllPanes(AnimationName(layoutName, "FocusIn"));
    focusOutAnimation = layout->BindAnimationToAllPanes(AnimationName(layoutName, "FocusOut"));
    selectAnimation = layout->BindAnimationToAllPanes(AnimationName(layoutName, "SelectLetter"));
    leaveAnimation = layout->BindAnimationToAllPanes(AnimationName(layoutName, "ExitLetter"));

    std::uint64_t boardTicks = stored.message.boardTime;
    if (boardTicks <= ticksNow)
    {
        bool isNew = ticksNow - boardTicks < NewForSeconds * ConsoleTicksPerSecond;
        pinAnimation = layout->BindAnimation(AnimationName(layoutName, isNew ? "NewAnim" : "DefAnim"), PinGroup);
        layout->StartAnimation(pinAnimation);
    }

    layout->SetText(LetterText, ShortTextOf(stored.message));
    if (Formats::Pane* pictureFrame = layout->FindPane(PictureFrame))
        pictureFrame->isVisible = false;
    ShowTheMii(portraits);
    Calculate();
}

void SmallLetter::ShowTheMii(MiiPortraits& portraits)
{
    Formats::Pane& miiPicture = layout->PaneNamed(MiiPicture);
    miiPicture.isVisible = false;
    if (!stored.message.mii)
        return;

    std::optional<Layout::LoadedTexture> face = portraits.FaceOf(*stored.message.mii);
    if (!face)
        return;

    layout->ShowTexture(MiiPicture, *face);
    miiPicture.isVisible = true;
}

StoredMessage& SmallLetter::Message()
{
    return stored;
}

Formats::Vector2 SmallLetter::PlaceOnTheDay() const
{
    return Formats::Vector2{stored.message.positionX, stored.message.positionY};
}

void SmallLetter::MoveOnTheDay(Formats::Vector2 place)
{
    stored.message.positionX = std::clamp(place.x, LeftmostLetterPlace, RightmostLetterPlace);
    stored.message.positionY = std::clamp(place.y, LowestLetterPlace, HighestLetterPlace);
}

void SmallLetter::PutDayAt(Formats::Vector2 place)
{
    dayPlace = place;
}

bool SmallLetter::IsPointedAt(Formats::Vector2 place)
{
    std::vector<const Formats::Pane*> buttons = layout->BoundingPanesAt(place);
    return std::any_of(buttons.begin(), buttons.end(), [](const Formats::Pane* pane) { return pane->name == LetterButton; });
}

void SmallLetter::Paste()
{
    layout->StartAnimation(pasteAnimation);
}

void SmallLetter::Focus()
{
    layout->StartAnimation(focusInAnimation);
}

void SmallLetter::Unfocus()
{
    layout->StartAnimation(focusOutAnimation);
}

void SmallLetter::Select()
{
    layout->StartAnimation(selectAnimation);
}

void SmallLetter::Leave()
{
    layout->StartAnimation(leaveAnimation);
}

bool SmallLetter::IsMoving() const
{
    for (std::size_t slot : {pasteAnimation, selectAnimation, leaveAnimation})
    {
        if (layout->IsAnimationPlaying(slot))
            return true;
    }
    return false;
}

void SmallLetter::Calculate()
{
    MoveLayoutTo(*layout, Formats::Vector2{dayPlace.x + stored.message.positionX, dayPlace.y + stored.message.positionY});
    layout->Calculate();
}

void SmallLetter::Draw()
{
    layout->Draw();
}

std::u16string SmallLetter::ShortTextOf(const Formats::BoardMessage& message)
{
    const std::u16string& text = message.kind == Formats::BoardMessageKind::Memo ? message.body : message.subject;
    std::u16string firstLine = text.substr(0, text.find_first_of(u"\r\n"));
    if (firstLine.size() <= ShortTextLength)
        return firstLine;
    return firstLine.substr(0, ShortTextLength) + std::u16string(Ellipsis);
}

}
