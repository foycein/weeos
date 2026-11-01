#pragma once

#include <cstddef>
#include <string>

namespace SystemMenu
{

// A rule says whether one character may be typed into a field.
using CharacterRule = bool (*)(char16_t character);

// Every character that the keyboard can make.
bool IsAnyCharacter(char16_t character);

// The digits 0 to 9, for a PIN or a number.
bool IsDigit(char16_t character);

struct TextFieldRules
{
    // The most characters the field holds. A character is one 16 bit unit of the text.
    std::size_t maximumLength = 0;

    CharacterRule allowedCharacters = IsAnyCharacter;

    // A field of one line refuses the line feed.
    bool allowsLineFeeds = false;
};

// The text that is typed, the cursor in it and the selected part of it (the text field of the soft keyboard).
// The cursor stands between two characters: position 0 is in front of the first one and the length of the text is behind the last one.
class TextField
{
public:
    // Text that the rules would refuse, or that is longer than the limit, is cut down to what the rules take.
    TextField(const std::u16string& initialText, const TextFieldRules& fieldRules);

    const std::u16string& Text() const;
    std::size_t CursorPosition() const;

    // The selected characters, from the first one up to, but not including, the end. Both are the cursor when nothing is selected.
    std::size_t SelectionStart() const;
    std::size_t SelectionEnd() const;
    bool HasSelection() const;

    // True when Insert would take the character now: the rules allow it, and there is room for it once the selected text is gone.
    bool CanTake(char16_t character) const;

    // Types a character at the cursor. It replaces the selected text. Gives false, and changes nothing, when the field does not take it.
    bool Insert(char16_t character);

    // The key that deletes the character in front of the cursor, and the one that deletes the character behind it.
    // Both delete the selected text instead when there is some. They give false when there is nothing to delete.
    bool DeleteBeforeCursor();
    bool DeleteAtCursor();

    // Moves the cursor. The selection is then the text between the place where the movement started and the cursor,
    // or no selection when the movement does not extend it. A position beyond the end of the text is the end.
    void MoveCursorTo(std::size_t position, bool extendsSelection);
    void MoveCursorLeft(bool extendsSelection);
    void MoveCursorRight(bool extendsSelection);

    void SelectAll();

private:
    void DeleteSelection();

    TextFieldRules rules;
    std::u16string text;
    std::size_t cursor = 0;

    // The other end of the selection. It equals the cursor when nothing is selected.
    std::size_t selectionAnchor = 0;
};

}
