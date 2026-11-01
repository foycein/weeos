#pragma once

#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakFiles.h"

#include "SystemMenu/SoundRequests.h"

#include <optional>
#include <string>
#include <vector>

namespace Channels
{

// The message window of the Wii Speak Channel (layout 01030_dialog of system.szs, 0x8006CC84 of the program): a text in a window
// over the darkened screen, with one button in the middle, two buttons side by side, or no button and a turning circle while
// the channel waits for something.
class WiiSpeakDialog
{
public:
    // The display and the files must outlive the dialog.
    WiiSpeakDialog(SystemMenu::Display& display, const WiiSpeakFiles& files);

    // Opens the window with the text and the labels of its buttons: none for the waiting circle, one, or two (left, then right).
    void Show(const std::u16string& text, const std::vector<std::u16string>& buttonLabels);

    // Closes a window, also one that waits.
    void Close();

    bool IsOpen() const;

    // The button that was pressed (0 for the single or the left one, 1 for the right one), once, after the window has closed.
    std::optional<int> TakeChoice();

    void Update(const SystemMenu::InputManager& input, SystemMenu::SoundRequests& sounds);

    SceneLayout& Layout();
    void Draw();

private:
    enum class Phase
    {
        Closed,
        Open,
        Closing,
    };

    void ShowButton(int buttonNumber, bool isShown);

    SceneLayout layout;
    Phase phase = Phase::Closed;
    int buttonCount = 0;
    std::optional<int> pendingChoice;
    std::optional<int> finishedChoice;
};

}
