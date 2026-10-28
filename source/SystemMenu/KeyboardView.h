#pragma once

#include "SystemMenu/ButtonController.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MenuLayout.h"

#include "SystemMenu/UsKeyboardLayout.h"

#include "Layout/LayoutArchive.h"
#include "Text/MessageCatalog.h"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace SystemMenu
{

// The keys and buttons of the soft keyboard.
enum class KeyKind
{
    // One of the 50 keys of the grid that type a character. The number of the key is its place in the grid.
    Character,

    Delete,
    Enter,
    Shift,
    CapsLock,
    Space,

    // The button that opens the symbol window.
    Symbols,

    // The two buttons of the bar at the bottom.
    Cancel,
    Confirm,

    // One of the 20 keys of the symbol window, and its three buttons.
    Symbol,
    CloseSymbols,
    PreviousSymbolPage,
    NextSymbolPage,

    // The arrows next to the text field that scroll its lines.
    ScrollUp,
    ScrollDown,
};

struct KeyId
{
    KeyKind kind;

    // The number of a Character key or of a Symbol key. It is 0 for the other kinds.
    int number = 0;

    bool operator==(const KeyId&) const = default;
};

// What happened to a key: a pointer moved onto it or off it, or a player pressed it with A.
struct KeyEvent
{
    ButtonEventKind kind;
    KeyId key;
    int playerIndex;
};

enum class KeyAnimation
{
    // The key grows while a pointer is on it, and shrinks when the pointer leaves.
    FocusIn,
    FocusOut,

    // The key was pressed.
    Pushed,

    // The key of Shift or Caps is down, or has come up.
    ToggleOn,
    ToggleOff,
};

// What the text field of the keyboard shows.
struct FieldLook
{
    std::u16string title;
    std::u16string text;
    std::size_t cursor = 0;
    std::size_t selectionStart = 0;
    std::size_t selectionEnd = 0;
    bool isCaretShown = true;
};

// What the soft keyboard looks like (the layouts of sofkeybd.ash: background, text field, keys, bar and symbol window) and where its keys are.
// It knows nothing about typing: the keyboard tells it what to show and asks it what the pointers did.
class KeyboardView
{
public:
    // The number of lines of the text field that can be seen, and of the keys in the symbol window.
    static constexpr int VisibleLineCount = 2;
    static constexpr int SymbolsPerPage = 20;

    // The display, the files and the messages must stay alive as long as the view.
    KeyboardView(Display& display, const MenuFiles& files, const Text::MessageCatalog& messages);

    // The keyboard fades in and out. The opacity goes from 0 (gone) to 255 (all there).
    void SetOpacity(std::uint8_t opacity);

    // The buttons see the pointers only while they are enabled: the keyboard enables them while it is open, and the keys of the grid
    // only while the symbol window is closed.
    void EnableButtons(bool areEnabled);
    void OpenSymbolWindow(bool shouldBeOpen);
    bool IsSymbolWindowOpen() const;

    // The labels of the keys of the grid. Shift shows the signs above the keys, and shows the letters in capitals, and so does the caps lock
    // but for the signs.
    void ShowCharacters(bool isShiftOn, bool isCapsLockOn);

    // A key that types no character in the field is dimmed and does not react.
    void ShowCharacterKeyAllowed(int characterKeyNumber, bool isAllowed);

    // The symbols of the page of the symbol window, at most SymbolsPerPage. The keys without a symbol are hidden.
    void ShowSymbolPage(const std::u16string& symbols, int pageNumber, int pageCount);

    // The text field. The view scrolls its lines so that the cursor can be seen.
    void ShowField(const FieldLook& look);

    // Scrolls the lines of the text field. The scroll stays as it is until the field changes.
    void ScrollField(int lineDistance);

    // The position that the cursor gets when it moves up or down by whole lines, with the column of the characters kept as near as it can be.
    std::size_t CursorPositionAfterMoving(const std::u16string& text, std::size_t cursor, int lineDistance);

    void PlayKeyAnimation(KeyId key, KeyAnimation animation);

    // The events of the pointers since the last poll.
    std::vector<KeyEvent> Poll(const InputManager& input);

    // One step: moves the animations.
    void Update();

    void Draw();

private:
    // A key or button of the layouts: the bounding pane that the pointers hit, the picture that the animations move,
    // and the names of its animation files without their endings.
    struct KeyRecord
    {
        KeyId id;
        MenuLayout* layout;
        ButtonController* buttons;
        std::string buttonPane;
        std::string picturePane;
        std::string animationPrefix;
    };

    using Rectangle = std::pair<Formats::Vector2, Formats::Vector2>;

    void HideWhatWeeOSDoesNotNeed();
    void SetFixedTexts();
    void RegisterKeys();
    void RegisterKey(KeyId id, MenuLayout& layout, ButtonController& buttons, const std::string& buttonPane, const std::string& picturePane,
        const std::string& animationPrefix);
    void EnableButtonsThatCanBePressed();
    const KeyRecord& RecordOf(KeyId key) const;
    void StartPictureAnimation(MenuLayout& layout, const std::string& picturePane, const std::string& animationName);
    void ShowScrollArrow(const std::string& picturePane, bool shouldBeShown, bool& isShown);

    int LineOfCharacter(const Text::LaidOutText& text, std::size_t characterNumber) const;
    void ScrollToShowCursor(const Text::LaidOutText& text, std::size_t cursor);
    void ShowVisibleLines(const FieldLook& look, const Text::LaidOutText& fullText);
    std::vector<Rectangle> AreasOfCharactersInPicture(const Text::LaidOutText& text, std::size_t first, std::size_t end);
    void DrawFieldMarks();
    void DrawRectangle(const Rectangle& area, Formats::Rgba8Color color);

    Display& display;
    const Text::MessageCatalog& messages;
    std::unique_ptr<Layout::LayoutArchive> archive;

    std::unique_ptr<MenuLayout> background;
    std::unique_ptr<MenuLayout> field;
    std::unique_ptr<MenuLayout> keys;
    std::unique_ptr<MenuLayout> bar;
    std::unique_ptr<MenuLayout> symbols;

    std::unique_ptr<ButtonController> fieldButtons;
    std::unique_ptr<ButtonController> keyButtons;
    std::unique_ptr<ButtonController> barButtons;
    std::unique_ptr<ButtonController> symbolButtons;

    std::vector<KeyRecord> records;
    std::map<std::string, std::size_t> recordOfButtonPane;
    std::map<std::pair<std::string, std::string>, std::size_t> slotOfAnimation;

    // The animation that plays on a picture now. A new animation of the picture stops it.
    std::map<std::string, std::size_t> playingSlotOfPicture;

    bool areButtonsEnabled = false;
    bool isSymbolWindowOpen = false;
    bool isFullyShown = false;
    bool isCaretShown = false;
    std::array<bool, CharacterKeyCount> isCharacterKeyAllowed;
    int symbolCountOfPage = 0;
    int symbolPageCount = 0;
    std::size_t symbolWindowAppearSlot = 0;
    std::size_t symbolWindowDisappearSlot = 0;

    // The lines of the text field: how many there are, and the first one that can be seen.
    int lineCount = 0;
    int firstVisibleLine = 0;
    bool isScrollUpShown = false;
    bool isScrollDownShown = false;
    std::u16string shownText;
    std::size_t shownCursor = 0;

    // Where the caret and the selection are in the picture, as the last ShowField found them.
    std::vector<Rectangle> selectionAreas;
    std::optional<Rectangle> caretArea;
};

}
