#pragma once

#include "Platform/WindowListener.h"
#include "Platform/WindowSize.h"

#include <windows.h>

#include <string_view>

namespace Platform
{

constexpr WindowSize DefaultClientSize = {1280, 720};

// The smallest area inside the frame that the user can drag the window to.
constexpr WindowSize SmallestClientSize = {480, 270};

// The number of the program's icon in the resources of WeeOS.exe (source/WeeOS/WeeOS.rc). A program without it gets the icon of Windows.
constexpr int ProgramIconResource = 1;

// Only WeeOS.exe started by the owner shows its window. Tests and the runs of helpers draw into a window that is never shown, so nothing
// appears on the screen (docs/HelperRules.md).
enum class WindowVisibility
{
    Hidden,
    Shown,
};

// Where the window lies on the desktop when it is not fullscreen: the top left corner of its frame and the size of the area inside.
struct WindowPlace
{
    int left = 0;
    int top = 0;
    WindowSize clientSize;

    bool operator==(const WindowPlace& other) const = default;
};

// The window of the program. The Windows cursor is hidden while it is over the window, and the mouse is never captured.
// While the user drags its frame the area inside stays 16:9. F11 and Alt+Enter switch between the window and borderless fullscreen.
// Windows sends the messages of the window while the program calls ProcessPendingMessages.
class Window
{
public:
    // The client size is the size of the area inside the window frame.
    explicit Window(std::wstring_view title, WindowSize clientSize = DefaultClientSize, WindowVisibility visibility = WindowVisibility::Hidden);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // The listener gets the events from now on. It must outlive its time as listener. A null listener stops the events.
    void SetListener(WindowListener* newListener);

    bool IsShown() const;

    // The size of the area inside the window frame, in pixels. It is zero while the window is minimized.
    WindowSize ClientSize() const;
    bool IsMinimized() const;

    // Gives the area inside the window frame this size. A window in fullscreen keeps covering the monitor and gets the size when it leaves.
    void SetClientSize(WindowSize clientSize);

    // The place of the window when it is not fullscreen, also while it is fullscreen or minimized.
    WindowPlace WindowedPlace() const;

    // Puts the window at this place, or keeps the place for later while the window is fullscreen. A place that lies on no monitor is
    // not taken.
    void SetWindowedPlace(const WindowPlace& place);

    // Borderless fullscreen covers the whole monitor the window is on.
    bool IsFullscreen() const;
    void ToggleFullscreen();

    // True once after the user moved or resized the window or switched fullscreen with a key, so that the program can remember it.
    bool TakePlaceChange();

    // True once after the user asked to close the window (the close button or Alt+F4). The window stays open until it is destroyed,
    // so that the program can ask first.
    bool TakeCloseRequest();

    HWND NativeHandle() const;

private:
    static void RegisterWindowClass();
    static LRESULT CALLBACK WindowProcedure(HWND windowHandle, UINT message, WPARAM firstParameter, LPARAM secondParameter);

    LRESULT HandleMessage(UINT message, WPARAM firstParameter, LPARAM secondParameter);
    void HandleMouseMove(LPARAM mousePosition);
    void HandleMouseWheel(WPARAM wheelInformation);
    void HandleKey(WPARAM virtualKeyCode, LPARAM keyInformation, bool isPressed);
    bool IsFullscreenKey(UINT message, WPARAM virtualKeyCode, LPARAM keyInformation) const;
    void KeepSixteenByNine(WPARAM draggedEdge, RECT& edges) const;
    void SetSmallestSize(MINMAXINFO& limits) const;
    WindowSize FrameSize() const;
    void EnterFullscreen();
    void LeaveFullscreen();
    DWORD StyleWithVisibility(DWORD style) const;

    HWND handle = nullptr;
    WindowListener* listener = nullptr;
    bool isShown = false;
    bool closeWasRequested = false;
    bool isFullscreen = false;
    bool placeHasChanged = false;
    bool isWaitingForMouseLeave = false;
    int wheelTurnSinceLastNotch = 0;
    WINDOWPLACEMENT windowedPlacement = {};
};

}
