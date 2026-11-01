#pragma once

#include "Platform/WindowListener.h"
#include "Platform/WindowSize.h"

#include <windows.h>

#include <string_view>

namespace Platform
{

constexpr WindowSize DefaultClientSize = {1280, 720};

// The window of the program. The Windows cursor is hidden while it is over the window, and the mouse is never captured.
// Windows sends the messages of the window while the program calls ProcessPendingMessages.
class Window
{
public:
    // The client size is the size of the area inside the window frame.
    explicit Window(std::wstring_view title, WindowSize clientSize = DefaultClientSize);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // The listener gets the events from now on. It must outlive its time as listener. A null listener stops the events.
    void SetListener(WindowListener* newListener);

    // The size of the area inside the window frame, in pixels. It is zero while the window is minimized.
    WindowSize ClientSize() const;
    bool IsMinimized() const;

    // Gives the area inside the window frame this size. A window in fullscreen keeps covering the monitor.
    void SetClientSize(WindowSize clientSize);

    // Borderless fullscreen covers the whole monitor the window is on.
    bool IsFullscreen() const;
    void ToggleFullscreen();

    // True once the user asked to close the window (the close button or Alt+F4). The window stays open until it is destroyed.
    bool IsCloseRequested() const;

    HWND NativeHandle() const;

private:
    static void RegisterWindowClass();
    static LRESULT CALLBACK WindowProcedure(HWND windowHandle, UINT message, WPARAM firstParameter, LPARAM secondParameter);

    LRESULT HandleMessage(UINT message, WPARAM firstParameter, LPARAM secondParameter);
    void HandleMouseMove(LPARAM mousePosition);
    void HandleKey(WPARAM virtualKeyCode, LPARAM keyInformation, bool isPressed);
    void EnterFullscreen();
    void LeaveFullscreen();

    HWND handle = nullptr;
    WindowListener* listener = nullptr;
    bool closeWasRequested = false;
    bool isFullscreen = false;
    bool isWaitingForMouseLeave = false;
    WINDOWPLACEMENT windowedPlacement = {};
};

}
