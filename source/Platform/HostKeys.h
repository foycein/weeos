#pragma once

#include "Platform/KeyCodes.h"
#include "Platform/WindowListener.h"

#include <array>
#include <set>

namespace Platform
{

// What the user asks of the console with the keys of the PC: the POWER button and the RESET button on the front of the console,
// and the slot of the SD card. There is no EJECT button here, because the disc slot stays empty (docs/DECISIONS.md, scope).
enum class HostCommand
{
    Power,
    Reset,

    // Puts the SD card into the slot, or takes it out.
    ToggleSdCard,
};

class HostCommandListener
{
public:
    virtual ~HostCommandListener() = default;

    virtual void OnHostCommand(HostCommand command) = 0;
};

// Control and this key give this command. The window's close button and Alt+F4 are not keys: the program counts them as the POWER button.
// Assumed (docs/audit/OpenPoints/Platform.md, PLT-01 and PLT-02): the original has no keys, the console has buttons and a slot.
struct HostKeyChord
{
    int virtualKeyCode;
    HostCommand command;
};

inline constexpr auto HostKeyChords = std::to_array<HostKeyChord>({
    {'Q', HostCommand::Power},
    {'R', HostCommand::Reset},
    {'S', HostCommand::ToggleSdCard},
});

// A window listener that turns the chords of HostKeyChords into host commands and passes everything else on to the next listener.
// A chord is not passed on, neither its key going down nor its key going up, so the program behind it never sees the letter.
// The control keys are passed on. A chord needs a control key and no alt key: the AltGr key of many keyboards is a control key
// and an alt key together, and AltGr and a letter write a sign.
//
// Like every window listener it is used from the thread of the window only.
class HostKeys : public WindowListener
{
public:
    HostKeys(WindowListener& nextListener, HostCommandListener& commandListener);

    void OnMouseMoved(int x, int y) override;
    void OnMouseLeft() override;
    void OnMouseButton(MouseButton button, bool isPressed) override;
    void OnKey(int virtualKeyCode, bool isPressed) override;
    void OnFocusLost() override;

private:
    void NoteModifierKey(int virtualKeyCode, bool isPressed);
    bool AChordIsActive() const;

    WindowListener& nextListener;
    HostCommandListener& commandListener;

    std::set<int> controlKeysDown;
    std::set<int> altKeysDown;

    // The keys that went down as a chord. Their release is not passed on either.
    std::set<int> chordKeysDown;
};

}
