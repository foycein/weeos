#include "SystemMenu/TextField.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

constexpr char16_t LineFeed = u'\n';

}

bool IsAnyCharacter([[maybe_unused]] char16_t character)
{
    return true;
}

bool IsDigit(char16_t character)
{
    return character >= u'0' && character <= u'9';
}

TextField::TextField(const std::u16string& initialText, const TextFieldRules& fieldRules)
    : rules(fieldRules)
{
    for (char16_t character : initialText)
    {
        if (CanTake(character))
            text.push_back(character);
    }
    cursor = text.size();
    selectionAnchor = cursor;
}

const std::u16string& TextField::Text() const
{
    return text;
}

std::size_t TextField::CursorPosition() const
{
    return cursor;
}

std::size_t TextField::SelectionStart() const
{
    return std::min(cursor, selectionAnchor);
}

std::size_t TextField::SelectionEnd() const
{
    return std::max(cursor, selectionAnchor);
}

bool TextField::HasSelection() const
{
    return cursor != selectionAnchor;
}

bool TextField::CanTake(char16_t character) const
{
    if (!rules.allowedCharacters(character))
        return false;

    if (character == LineFeed && !rules.allowsLineFeeds)
        return false;

    std::size_t selectedCount = SelectionEnd() - SelectionStart();
    return text.size() - selectedCount < rules.maximumLength;
}

bool TextField::Insert(char16_t character)
{
    if (!CanTake(character))
        return false;

    DeleteSelection();
    text.insert(cursor, 1, character);
    ++cursor;
    selectionAnchor = cursor;
    return true;
}

bool TextField::DeleteBeforeCursor()
{
    if (HasSelection())
    {
        DeleteSelection();
        return true;
    }

    if (cursor == 0)
        return false;

    text.erase(cursor - 1, 1);
    --cursor;
    selectionAnchor = cursor;
    return true;
}

bool TextField::DeleteAtCursor()
{
    if (HasSelection())
    {
        DeleteSelection();
        return true;
    }

    if (cursor == text.size())
        return false;

    text.erase(cursor, 1);
    return true;
}

void TextField::DeleteSelection()
{
    std::size_t start = SelectionStart();
    text.erase(start, SelectionEnd() - start);
    cursor = start;
    selectionAnchor = start;
}

void TextField::MoveCursorTo(std::size_t position, bool extendsSelection)
{
    cursor = std::min(position, text.size());
    if (!extendsSelection)
        selectionAnchor = cursor;
}

void TextField::MoveCursorLeft(bool extendsSelection)
{
    if (!extendsSelection && HasSelection())
    {
        MoveCursorTo(SelectionStart(), false);
        return;
    }

    if (cursor > 0)
        MoveCursorTo(cursor - 1, extendsSelection);
}

void TextField::MoveCursorRight(bool extendsSelection)
{
    if (!extendsSelection && HasSelection())
    {
        MoveCursorTo(SelectionEnd(), false);
        return;
    }

    MoveCursorTo(cursor + 1, extendsSelection);
}

void TextField::SelectAll()
{
    selectionAnchor = 0;
    cursor = text.size();
}

}
