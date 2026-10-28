#include "SystemMenu/SoftKeyboard.h"

#include "SystemMenu/UsKeyboardLayout.h"

#include <algorithm>
#include <cmath>

namespace SystemMenu
{

namespace
{

// The caret blinks: it is there for 0.6 seconds of every second, and it starts a new second whenever the text changes.
constexpr double CaretCycleSeconds = 1.0;
constexpr double CaretShownSeconds = 0.6;

constexpr std::uint8_t FullOpacity = 255;

// The buttons of the D-pad of the Wii Remote and of the Classic Controller move the cursor.
constexpr ButtonSet LeftButtons = {Input::RemoteButtons::Left, Input::ClassicButtons::Left};
constexpr ButtonSet RightButtons = {Input::RemoteButtons::Right, Input::ClassicButtons::Right};
constexpr ButtonSet UpButtons = {Input::RemoteButtons::Up, Input::ClassicButtons::Up};
constexpr ButtonSet DownButtons = {Input::RemoteButtons::Down, Input::ClassicButtons::Down};

int PageCountOfSymbols()
{
    int symbolCount = static_cast<int>(SymbolsOfTheSymbolWindow().size());
    return (symbolCount + KeyboardView::SymbolsPerPage - 1) / KeyboardView::SymbolsPerPage;
}

}

SoftKeyboard::SoftKeyboard(Display& display, const MenuFiles& files, const Text::MessageCatalog& messages, const MenuClock& menuClock,
    InputManager& menuInput, SoundRequests& menuSounds)
    : clock(menuClock)
    , input(menuInput)
    , sounds(menuSounds)
    , view(display, files, messages)
{
    view.SetOpacity(0);
}

void SoftKeyboard::UsePhysicalKeyboard(Ios::KeyboardService& service)
{
    physicalKeyboard = std::make_unique<PhysicalKeyboard>(service);
}

void SoftKeyboard::Open(const KeyboardRequest& newRequest)
{
    request = newRequest;
    field.emplace(request.initialText, request.rules);
    answer.reset();

    isShiftOn = false;
    isCapsLockOn = false;
    symbolPage = 0;
    view.OpenSymbolWindow(false);
    view.EnableButtons(false);
    ShowKeysInView();
    NoteThatTheFieldChanged();
    ShowFieldInView();

    sounds.Request(KeyboardSoundName::Open);
    StartPhase(Phase::FadingIn);
}

bool SoftKeyboard::IsOpen() const
{
    return phase != Phase::Closed;
}

std::optional<KeyboardAnswer> SoftKeyboard::TakeAnswer()
{
    if (phase != Phase::Closed)
        return std::nullopt;

    std::optional<KeyboardAnswer> givenAnswer = answer;
    answer.reset();
    return givenAnswer;
}

void SoftKeyboard::StartPhase(Phase newPhase)
{
    phase = newPhase;
    phaseStartSeconds = clock.Seconds();
}

void SoftKeyboard::Update()
{
    // The keys of the USB keyboard are taken while the keyboard is closed too, so that they do not wait for the next field.
    std::vector<TypingCommand> commands;
    if (physicalKeyboard)
        commands = physicalKeyboard->Poll(clock.Seconds(), isCapsLockOn);

    switch (phase)
    {
    case Phase::Closed:
        return;
    case Phase::FadingIn:
        FadeIn();
        break;
    case Phase::Open:
        HandlePointers();
        HandleDirectionButtons();
        for (const TypingCommand& command : commands)
        {
            if (phase == Phase::Open)
                ApplyTypingCommand(command);
        }
        break;
    case Phase::FadingOut:
        FadeOut();
        break;
    }

    if (phase == Phase::Closed)
        return;

    ShowFieldInView();
    view.Update();
}

void SoftKeyboard::FadeIn()
{
    double progress = (clock.Seconds() - phaseStartSeconds) / SecondsToFade;
    if (progress < 1)
    {
        view.SetOpacity(static_cast<std::uint8_t>(progress * FullOpacity));
        return;
    }

    view.SetOpacity(FullOpacity);
    view.EnableButtons(true);
    StartPhase(Phase::Open);
}

void SoftKeyboard::FadeOut()
{
    double progress = (clock.Seconds() - phaseStartSeconds) / SecondsToFade;
    if (progress < 1)
    {
        view.SetOpacity(static_cast<std::uint8_t>((1 - progress) * FullOpacity));
        return;
    }

    view.SetOpacity(0);
    StartPhase(Phase::Closed);
}

void SoftKeyboard::Draw()
{
    if (phase != Phase::Closed)
        view.Draw();
}

// ---- The pointer

void SoftKeyboard::HandlePointers()
{
    for (const KeyEvent& event : view.Poll(input))
    {
        if (event.playerIndex != request.playerIndex)
            continue;

        HandleKeyEvent(event);
        if (phase != Phase::Open)
            return;
    }
}

void SoftKeyboard::HandleKeyEvent(const KeyEvent& event)
{
    if (event.kind == ButtonEventKind::FocusGained)
    {
        sounds.Request(KeyboardSoundName::KeyFocused);
        view.PlayKeyAnimation(event.key, KeyAnimation::FocusIn);
    }
    else if (event.kind == ButtonEventKind::FocusLost)
    {
        view.PlayKeyAnimation(event.key, KeyAnimation::FocusOut);
    }
    else
    {
        PressKey(event.key);
    }
}

void SoftKeyboard::PressKey(KeyId key)
{
    view.PlayKeyAnimation(key, KeyAnimation::Pushed);

    switch (key.kind)
    {
    case KeyKind::Character:
        Type(CharacterOfGridKey(key.number, isShiftOn, isCapsLockOn));
        if (isShiftOn)
            ToggleShift();
        break;
    case KeyKind::Delete:
        DeleteBeforeCursor();
        break;
    case KeyKind::Enter:
        PressEnter();
        break;
    case KeyKind::Shift:
        ToggleShift();
        break;
    case KeyKind::CapsLock:
        ToggleCapsLock();
        break;
    case KeyKind::Space:
        Type(u' ');
        break;
    case KeyKind::Symbols:
        OpenSymbolWindow();
        break;
    case KeyKind::Cancel:
        Finish(false);
        break;
    case KeyKind::Confirm:
        Finish(true);
        break;
    case KeyKind::Symbol:
        Type(SymbolsOfTheSymbolWindow().at(symbolPage * KeyboardView::SymbolsPerPage + key.number));
        break;
    case KeyKind::CloseSymbols:
        view.OpenSymbolWindow(false);
        sounds.Request(KeyboardSoundName::ButtonPressed);
        break;
    case KeyKind::PreviousSymbolPage:
        ChangeSymbolPage(-1);
        break;
    case KeyKind::NextSymbolPage:
        ChangeSymbolPage(1);
        break;
    case KeyKind::ScrollUp:
        view.ScrollField(-1);
        sounds.Request(KeyboardSoundName::LinesScrolled);
        break;
    case KeyKind::ScrollDown:
        view.ScrollField(1);
        sounds.Request(KeyboardSoundName::LinesScrolled);
        break;
    }
}

void SoftKeyboard::HandleDirectionButtons()
{
    if (input.HasPlayerPressed(request.playerIndex, LeftButtons))
        MoveCursor(0, -1, false);

    if (input.HasPlayerPressed(request.playerIndex, RightButtons))
        MoveCursor(0, 1, false);

    if (input.HasPlayerPressed(request.playerIndex, UpButtons))
        MoveCursor(-1, 0, false);

    if (input.HasPlayerPressed(request.playerIndex, DownButtons))
        MoveCursor(1, 0, false);
}

// ---- Typing

void SoftKeyboard::ApplyTypingCommand(const TypingCommand& command)
{
    switch (command.kind)
    {
    case TypingCommandKind::Character:
        Type(command.character);
        break;
    case TypingCommandKind::Backspace:
        DeleteBeforeCursor();
        break;
    case TypingCommandKind::Delete:
        sounds.Request(field->DeleteAtCursor() ? KeyboardSoundName::CharacterDeleted : KeyboardSoundName::NothingToDelete);
        NoteThatTheFieldChanged();
        break;
    case TypingCommandKind::MoveLeft:
        MoveCursor(0, -1, command.extendsSelection);
        break;
    case TypingCommandKind::MoveRight:
        MoveCursor(0, 1, command.extendsSelection);
        break;
    case TypingCommandKind::MoveUp:
        MoveCursor(-1, 0, command.extendsSelection);
        break;
    case TypingCommandKind::MoveDown:
        MoveCursor(1, 0, command.extendsSelection);
        break;
    case TypingCommandKind::MoveToStart:
        field->MoveCursorTo(0, command.extendsSelection);
        NoteThatTheFieldChanged();
        break;
    case TypingCommandKind::MoveToEnd:
        field->MoveCursorTo(field->Text().size(), command.extendsSelection);
        NoteThatTheFieldChanged();
        break;
    case TypingCommandKind::SelectAll:
        field->SelectAll();
        NoteThatTheFieldChanged();
        break;
    case TypingCommandKind::ToggleCapsLock:
        ToggleCapsLock();
        break;
    case TypingCommandKind::Enter:
        PressEnter();
        break;
    case TypingCommandKind::Cancel:
        Finish(false);
        break;
    }
}

void SoftKeyboard::Type(char16_t character)
{
    if (!field->Insert(character))
        return;

    sounds.Request(KeyboardSoundName::CharacterTyped);
    NoteThatTheFieldChanged();
}

void SoftKeyboard::DeleteBeforeCursor()
{
    sounds.Request(field->DeleteBeforeCursor() ? KeyboardSoundName::CharacterDeleted : KeyboardSoundName::NothingToDelete);
    NoteThatTheFieldChanged();
}

void SoftKeyboard::MoveCursor(int lineDistance, int characterDistance, bool extendsSelection)
{
    std::size_t cursorBefore = field->CursorPosition();

    if (lineDistance != 0)
    {
        std::size_t newCursor = view.CursorPositionAfterMoving(field->Text(), cursorBefore, lineDistance);
        field->MoveCursorTo(newCursor, extendsSelection);
    }
    else if (characterDistance < 0)
    {
        field->MoveCursorLeft(extendsSelection);
    }
    else
    {
        field->MoveCursorRight(extendsSelection);
    }

    if (field->CursorPosition() != cursorBefore)
        sounds.Request(KeyboardSoundName::CursorMoved);

    NoteThatTheFieldChanged();
}

// The Enter key makes a line feed in a field of several lines and answers OK in a field of one line.
void SoftKeyboard::PressEnter()
{
    if (request.rules.allowsLineFeeds)
        Type(u'\n');
    else
        Finish(true);
}

void SoftKeyboard::ToggleShift()
{
    isShiftOn = !isShiftOn;
    view.PlayKeyAnimation({KeyKind::Shift}, isShiftOn ? KeyAnimation::ToggleOn : KeyAnimation::ToggleOff);
    sounds.Request(isShiftOn ? KeyboardSoundName::LettersBecomeCapitals : KeyboardSoundName::LettersBecomeSmall);
    ShowKeysInView();
}

void SoftKeyboard::ToggleCapsLock()
{
    isCapsLockOn = !isCapsLockOn;
    view.PlayKeyAnimation({KeyKind::CapsLock}, isCapsLockOn ? KeyAnimation::ToggleOn : KeyAnimation::ToggleOff);
    sounds.Request(isCapsLockOn ? KeyboardSoundName::LettersBecomeCapitals : KeyboardSoundName::LettersBecomeSmall);
    ShowKeysInView();

    if (physicalKeyboard)
        physicalKeyboard->ShowCapsLock(isCapsLockOn);
}

void SoftKeyboard::OpenSymbolWindow()
{
    symbolPage = 0;
    ShowSymbolPageInView();
    view.OpenSymbolWindow(true);
    sounds.Request(KeyboardSoundName::SymbolWindowOpened);
}

void SoftKeyboard::ChangeSymbolPage(int pageDistance)
{
    int pageCount = PageCountOfSymbols();
    symbolPage = (symbolPage + pageDistance + pageCount) % pageCount;
    ShowSymbolPageInView();
    sounds.Request(KeyboardSoundName::SymbolPageChanged);
}

void SoftKeyboard::Finish(bool wasConfirmed)
{
    KeyboardAnswer finalAnswer;
    finalAnswer.wasConfirmed = wasConfirmed;
    finalAnswer.text = field->Text();
    answer = finalAnswer;

    sounds.Request(wasConfirmed ? KeyboardSoundName::DecideAndClose : KeyboardSoundName::CancelAndClose);
    view.EnableButtons(false);
    StartPhase(Phase::FadingOut);
}

// ---- What the user sees

void SoftKeyboard::ShowKeysInView()
{
    view.ShowCharacters(isShiftOn, isCapsLockOn);

    for (int keyNumber = 0; keyNumber < static_cast<int>(CharacterKeyCount); ++keyNumber)
    {
        char16_t character = CharacterOfGridKey(keyNumber, isShiftOn, isCapsLockOn);
        view.ShowCharacterKeyAllowed(keyNumber, request.rules.allowedCharacters(character));
    }
}

void SoftKeyboard::ShowSymbolPageInView()
{
    const std::u16string& allSymbols = SymbolsOfTheSymbolWindow();
    std::size_t firstSymbol = static_cast<std::size_t>(symbolPage) * KeyboardView::SymbolsPerPage;
    std::u16string pageSymbols = allSymbols.substr(firstSymbol, KeyboardView::SymbolsPerPage);
    view.ShowSymbolPage(pageSymbols, symbolPage + 1, PageCountOfSymbols());
}

void SoftKeyboard::ShowFieldInView()
{
    double secondsSinceChange = clock.Seconds() - fieldChangedSeconds;

    FieldLook look;
    look.title = request.title;
    look.text = field->Text();
    look.cursor = field->CursorPosition();
    look.selectionStart = field->SelectionStart();
    look.selectionEnd = field->SelectionEnd();
    look.isCaretShown = std::fmod(secondsSinceChange, CaretCycleSeconds) < CaretShownSeconds;
    view.ShowField(look);
}

void SoftKeyboard::NoteThatTheFieldChanged()
{
    fieldChangedSeconds = clock.Seconds();
}

}
