#include "Platform/HostKeys.h"

#include <optional>

namespace Platform
{

namespace
{

bool IsControlKey(int virtualKeyCode)
{
    return virtualKeyCode == KeyCode::Control || virtualKeyCode == KeyCode::LeftControl || virtualKeyCode == KeyCode::RightControl;
}

bool IsAltKey(int virtualKeyCode)
{
    return virtualKeyCode == KeyCode::Alt || virtualKeyCode == KeyCode::LeftAlt || virtualKeyCode == KeyCode::RightAlt;
}

void NoteKey(std::set<int>& keysDown, int virtualKeyCode, bool isPressed)
{
    if (isPressed)
        keysDown.insert(virtualKeyCode);
    else
        keysDown.erase(virtualKeyCode);
}

std::optional<HostCommand> CommandOfChordKey(int virtualKeyCode)
{
    for (const HostKeyChord& chord : HostKeyChords)
    {
        if (chord.virtualKeyCode == virtualKeyCode)
            return chord.command;
    }
    return std::nullopt;
}

}

HostKeys::HostKeys(WindowListener& followingListener, HostCommandListener& receivingListener)
    : nextListener(followingListener)
    , commandListener(receivingListener)
{
}

void HostKeys::OnMouseMoved(int x, int y)
{
    nextListener.OnMouseMoved(x, y);
}

void HostKeys::OnMouseLeft()
{
    nextListener.OnMouseLeft();
}

void HostKeys::OnMouseButton(MouseButton button, bool isPressed)
{
    nextListener.OnMouseButton(button, isPressed);
}

void HostKeys::OnKey(int virtualKeyCode, bool isPressed)
{
    NoteModifierKey(virtualKeyCode, isPressed);

    std::optional<HostCommand> command = CommandOfChordKey(virtualKeyCode);
    if (!command)
    {
        nextListener.OnKey(virtualKeyCode, isPressed);
        return;
    }

    bool isReleasing = !isPressed;
    if (isReleasing)
    {
        bool wentDownAsChord = chordKeysDown.erase(virtualKeyCode) > 0;
        if (!wentDownAsChord)
            nextListener.OnKey(virtualKeyCode, isPressed);
        return;
    }

    bool isHeldAlready = chordKeysDown.contains(virtualKeyCode);
    if (isHeldAlready)
        return;

    if (!AChordIsActive())
    {
        nextListener.OnKey(virtualKeyCode, isPressed);
        return;
    }

    chordKeysDown.insert(virtualKeyCode);
    commandListener.OnHostCommand(*command);
}

void HostKeys::OnFocusLost()
{
    controlKeysDown.clear();
    altKeysDown.clear();
    chordKeysDown.clear();
    nextListener.OnFocusLost();
}

// The modifier keys are passed on as they are. This only notes which of them are down.
void HostKeys::NoteModifierKey(int virtualKeyCode, bool isPressed)
{
    if (IsControlKey(virtualKeyCode))
        NoteKey(controlKeysDown, virtualKeyCode, isPressed);

    if (IsAltKey(virtualKeyCode))
        NoteKey(altKeysDown, virtualKeyCode, isPressed);
}

bool HostKeys::AChordIsActive() const
{
    return !controlKeysDown.empty() && altKeysDown.empty();
}

}
