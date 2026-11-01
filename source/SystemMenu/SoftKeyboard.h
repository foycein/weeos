#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/KeyboardView.h"
#include "SystemMenu/MenuClock.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/PhysicalKeyboard.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/TextField.h"

#include "Ios/Keyboard/KeyboardService.h"
#include "Text/MessageCatalog.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace SystemMenu
{

// The names of the sound effects of the soft keyboard in the sound archive (docs/research/SystemMenuStructure.md 4.12).
// The moment of each sound is ours: the original code was not read for it (docs/audit/OpenPoints/SystemMenu.md SYS-44).
namespace KeyboardSoundName
{

constexpr std::string_view Open = "WIPL_SE_SK_OPEN";
constexpr std::string_view DecideAndClose = "WIPL_SE_SK_DECIDE_CLOSE";
constexpr std::string_view CancelAndClose = "WIPL_SE_SK_CANCEL_CLOSE";
constexpr std::string_view KeyFocused = "WIPL_SE_CHAR_FOCUS";
constexpr std::string_view CharacterTyped = "WIPL_SE_CHAR_INPUT";
constexpr std::string_view CharacterDeleted = "WIPL_SE_CHAR_DELETE";
constexpr std::string_view NothingToDelete = "WIPL_SE_CHAR_DELETE_ERROR";
constexpr std::string_view CursorMoved = "WIPL_SE_CHAR_CURSOR";
constexpr std::string_view LettersBecomeCapitals = "WIPL_SE_CHAR_LARGER";
constexpr std::string_view LettersBecomeSmall = "WIPL_SE_CHAR_LOWER";
constexpr std::string_view LinesScrolled = "WIPL_SE_LINE_SCROLL";
constexpr std::string_view SymbolWindowOpened = "WIPL_SE_SYMBOL_PAGE_OPEN";
constexpr std::string_view SymbolPageChanged = "WIPL_SE_SK_SWITCHING_01";
constexpr std::string_view ButtonPressed = "WIPL_SE_CHAR_DECIDE";

}

// What a screen asks of the keyboard: a text field with its limits. Wii Settings asks for the nickname, the Message Board for a memo.
struct KeyboardRequest
{
    // The words above the text field, for example the message "Enter a nickname."
    std::u16string title;

    std::u16string initialText;

    // The most characters, the characters that are allowed, and whether the field has several lines.
    TextFieldRules rules;

    // The player whose pointer and D-pad work the keyboard. The pointers of the other players do nothing.
    int playerIndex = 0;
};

// What the user answered when the keyboard closed.
struct KeyboardAnswer
{
    // True for OK, false for Cancel. The text is the text of the field in both cases.
    bool wasConfirmed = false;
    std::u16string text;
};

// The soft keyboard of the menu (the object 0x81089098 of the original). A screen opens it with a request and asks every step
// whether the answer is there. The keyboard draws over the screens, takes the pointer of one player and the keys of a USB keyboard,
// and closes with OK or Cancel.
//
// Without prediction (docs/audit/OpenPoints/SystemMenu.md SYS-10): the keys type what they show.
class SoftKeyboard
{
public:
    // The keyboard fades in and out in ten steps.
    static constexpr int FadeStepCount = 10;
    static constexpr double SecondsToFade = FadeStepCount * MenuClock::SecondsPerStep;

    // The display, the files, the messages, the clock, the input and the sounds must stay alive as long as the keyboard.
    SoftKeyboard(Display& display, const MenuFiles& files, const Text::MessageCatalog& messages, const MenuClock& clock, InputManager& input,
        SoundRequests& sounds);

    // The keyboard of a channel, with the layouts of its own archive and its words (KeyboardView).
    SoftKeyboard(Display& display, std::unique_ptr<Layout::LayoutArchive> keyboardArchive, const KeyboardButtonTexts& texts,
        const MenuClock& clock, InputManager& input, SoundRequests& sounds);

    // The keys of this USB keyboard type into the field while the keyboard is open. The service must stay alive as long as the keyboard.
    void UsePhysicalKeyboard(Ios::KeyboardService& service);

    // Opens the keyboard for a field. A keyboard that is open already starts again with the new field.
    void Open(const KeyboardRequest& request);

    // True from Open until the keyboard has closed.
    bool IsOpen() const;

    // The answer, once, after the keyboard has closed. Nothing before that.
    std::optional<KeyboardAnswer> TakeAnswer();

    // One step: takes the pointers and the keys, and moves the keyboard on.
    void Update();

    void Draw();

private:
    enum class Phase
    {
        Closed,
        FadingIn,
        Open,
        FadingOut,
    };

    enum class CursorMove
    {
        OneCharacterLeft,
        OneCharacterRight,
        OneLineUp,
        OneLineDown,
    };

    void StartPhase(Phase newPhase);
    void FadeIn();
    void FadeOut();

    void HandlePointers();
    void HandleKeyEvent(const KeyEvent& event);
    void PressKey(KeyId key);
    void HandleDirectionButtons();
    void ApplyTypingCommand(const TypingCommand& command);

    void Type(char16_t character);
    void DeleteBeforeCursor();
    void DeleteAtCursor();
    void MoveCursor(CursorMove move, bool extendsSelection);
    void PressEnter();
    void ToggleShift();
    void ToggleCapsLock();
    void OpenSymbolWindow();
    void ChangeSymbolPage(int pageDistance);
    void Finish(bool wasConfirmed);

    void ShowKeysInView();
    void ShowSymbolPageInView();
    void ShowFieldInView();
    void NoteThatTheFieldChanged();

    const MenuClock& clock;
    InputManager& input;
    SoundRequests& sounds;
    KeyboardView view;
    std::unique_ptr<PhysicalKeyboard> physicalKeyboard;

    Phase phase = Phase::Closed;
    double phaseStartSeconds = 0;
    double fieldChangedSeconds = 0;

    KeyboardRequest request;
    std::optional<TextField> field;
    std::optional<KeyboardAnswer> answer;

    bool isShiftOn = false;
    bool isCapsLockOn = false;
    int symbolPage = 0;
};

}
