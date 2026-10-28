#pragma once

#include <cstddef>
#include <string>

namespace SystemMenu
{

enum class TextEntryState
{
    // No entry was started, or the one that was started has been taken.
    Idle,

    Typing,

    // The user ended the entry with the text that stands in Text().
    Finished,

    // The user left without a text.
    Cancelled,
};

// How a screen asks the user for a short text, for example the PIN of the parental controls. The soft keyboard of the menu is the
// finished form of this. The menu calls Update once in every step.
class TextEntry
{
public:
    TextEntry() = default;
    TextEntry(const TextEntry&) = delete;
    TextEntry& operator=(const TextEntry&) = delete;
    virtual ~TextEntry() = default;

    // Starts an entry for the player whose remote pointed at the button. The user types at most maximumCharacters characters.
    virtual void Begin(int playerIndex, std::size_t maximumCharacters) = 0;

    virtual void Update() = 0;

    virtual TextEntryState State() const = 0;

    // The characters typed so far, and the final text once the state is Finished.
    virtual const std::u16string& Text() const = 0;

    // Takes the result: the state is Idle again.
    virtual void End() = 0;
};

}
