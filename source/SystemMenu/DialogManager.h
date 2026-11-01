#pragma once

#include "SystemMenu/ButtonController.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/SoundRequests.h"

#include "Layout/LayoutArchive.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SystemMenu
{

// The five dialog layouts of dlgWdw.ash.
enum class DialogLayout
{
    // Only a message. It can show a turning wait icon or a progress bar.
    Message,

    OneButton,
    TwoButtons,
    ThreeButtons,

    // A wide message with two buttons.
    WideTwoButtons,
};

// What the user did, or what ended the dialog. The buttons are named like the panes of the layouts.
enum class DialogAnswer
{
    ButtonA,
    ButtonB,
    ButtonC,

    // The message dialog ran for the number of steps that was asked for.
    TimedOut,

    // The progress bar reached 100.
    ProgressDone,

    ClosedByProgram,
};

// One page of a dialog with pages.
struct DialogPage
{
    std::u16string message;

    // Button B turns to the next page, or closes the dialog on the last one. Button A turns back; a page without it hides it.
    std::u16string nextText;
    std::u16string backText;
    bool hasBackButton = false;

    // A layout of the screen that opened the dialog, shown this far above the middle of the dialog (0x81345EE8). The dialog places,
    // fades and calculates it while the page is shown; the screen must keep it alive and leave it alone meanwhile.
    MenuLayout* picture = nullptr;
    float pictureHeight = 0;

    // The animation in the first slot of the picture plays while the page is shown.
    bool playsPictureAnimation = false;
};

struct DialogRequest
{
    DialogLayout layout = DialogLayout::Message;

    // The texts of the dialog and of the buttons the layout has.
    std::u16string message;
    std::u16string buttonAText;
    std::u16string buttonBText;
    std::u16string buttonCText;

    // Message layout only. The dialog closes by itself after this many steps, 0 means it stays until the program closes it.
    std::uint32_t stepsBeforeClosing = 0;

    // Message layout only. The turning icon shows that the console is busy.
    bool showsWaitIcon = false;

    // Message layout only. A bar shows how far a job has come, see SetProgress. The picture behind the dialog is not darkened.
    bool showsProgress = false;

    // The button that sounds WIPL_SE_CANCEL when it is pressed; the others sound WIPL_SE_DECIDE. In the original the second button of a
    // window with two buttons cancels, and C of a window with three, unless the screen turns the two round (0x8134753C).
    DialogAnswer cancelButton = DialogAnswer::ButtonA;

    // A dialog of pages in the window with two buttons (0x81346C14): B turns to the next page and A back, the words fade out and the
    // next ones fade in. It closes with ButtonB after the last page and with ButtonA before the first. Layout, message and the button
    // texts above are not used.
    std::vector<DialogPage> pages;
};

// The dialog windows of the menu (the DialogManager of the original, docs/audit/SystemMenu.md). One dialog is open at a time: it opens
// with an animation, waits for a button, a time or a command, shows the button that was chosen and closes with an animation.
// A screen that waits for a dialog looks at IsOpen and at TakeAnswer.
// The sounds are the original's: a window opens with WIPL_SE_INFO_WINDOW, or with WIPL_SE_COPYING when it shows that the console is busy
// (0x8134644C), which ends with WIPL_SE_COPY_FINISH when the window closes (0x81345364); the hand on a button sounds
// WIPL_SE_BT_TARGETTING (0x81347284), a press WIPL_SE_DECIDE or WIPL_SE_CANCEL (0x8134753C).
class DialogManager
{
public:
    // The display, the files and the sound requests must outlive the dialog manager.
    DialogManager(Display& menuDisplay, const MenuFiles& files, SoundRequests& menuSounds);

    bool IsOpen() const;

    // Opens the dialog. Gives back false, and changes nothing, while another dialog is open.
    bool Open(const DialogRequest& request);

    // The progress bar moves two percent in every step towards the percentage, and the dialog ends when it shows 100.
    void SetProgress(int percentage);

    // Closes the dialog. The answer is ClosedByProgram. Nothing happens while no dialog is open.
    void Close();

    // The answer of the dialog that has closed. Nothing while a dialog is open or when the answer was taken already.
    std::optional<DialogAnswer> TakeAnswer();

    // Once in every step. The buttons react only while isActive is true, which is when the picture does not fade.
    void Update(const InputManager& input, bool isActive);

    void Draw();

private:
    enum class State
    {
        Closed,
        Opening,
        Waiting,
        ShowingChoice,
        TurningPage,
        Closing,
    };

    struct DialogWindow
    {
        std::unique_ptr<MenuLayout> layout;
        std::unique_ptr<ButtonController> buttons;
        std::size_t buttonCount = 0;
    };

    void MakeWindow(DialogLayout kind);
    void ShowTexts(const DialogRequest& request);
    void ShowMessageExtras(const DialogRequest& request);
    void UpdateWaiting(const InputManager& input, bool isActive);
    void UpdateMessageWindow();
    void ReactToButton(const ButtonEvent& event);
    void StartClosing(DialogAnswer reason);
    void OpenPage(std::size_t pageIndex);
    void ShowPageTexts(const DialogPage& page);
    void EndTheChoice();
    void UpdateTurningPage();
    void FadePage(const DialogPage& page, const DialogPage& otherPage, std::uint8_t alpha);
    void ShowBackButton(bool isShown);
    void UpdatePicture();
    const DialogPage* PageOfPicture() const;
    std::string_view SoundOfPress(DialogAnswer pressed) const;

    Display& display;
    SoundRequests& sounds;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::array<DialogWindow, 5> windows;
    DialogWindow* openWindow = nullptr;
    State state = State::Closed;
    std::size_t choiceSlot = 0;
    DialogAnswer closingAnswer = DialogAnswer::ClosedByProgram;
    std::optional<DialogAnswer> answer;

    bool closeRequested = false;
    std::uint32_t stepsBeforeClosing = 0;
    std::uint32_t stepsWaited = 0;
    bool showsProgress = false;
    bool soundsBusy = false;
    DialogAnswer cancelButton = DialogAnswer::ButtonA;
    int targetPercentage = 0;
    int shownPercentage = 0;

    // The buttons that a pointer is on.
    std::array<bool, 3> isButtonFocused = {};

    // A dialog with pages: the pages, the one shown, the one it turns from, and how far its words have faded in.
    std::vector<DialogPage> pages;
    std::size_t shownPage = 0;
    std::size_t previousPage = 0;
    bool isFadingOut = false;
    int pageAlpha = 0;
};

}
