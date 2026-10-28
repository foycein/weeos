#include "Platform/Window.h"

#include "Platform/PlatformError.h"

#include <cstdint>
#include <format>
#include <string>

namespace Platform
{

namespace
{

constexpr wchar_t WindowClassName[] = L"WeeOSWindow";
constexpr DWORD WindowedStyle = WS_OVERLAPPEDWINDOW;
constexpr DWORD FullscreenStyle = WS_POPUP;

// Windows keeps four bits of a system command for itself.
constexpr WPARAM SystemCommandMask = 0xFFF0;

// A key message has this bit set when the key was already down, which means the key is repeating.
constexpr LPARAM KeyWasDownBit = 1 << 30;

// Takes the events of a window that has no listener.
class IgnoringListener : public WindowListener
{
public:
    void OnMouseMoved(int, int) override
    {
    }

    void OnMouseLeft() override
    {
    }

    void OnMouseButton(MouseButton, bool) override
    {
    }

    void OnKey(int, bool) override
    {
    }

    void OnFocusLost() override
    {
    }
};

WindowListener* NobodyListening()
{
    static IgnoringListener ignoringListener;
    return &ignoringListener;
}

}

Window::Window(std::wstring_view title, WindowSize clientSize)
    : listener(NobodyListening())
{
    // Without this Windows stretches the picture of a window on a scaled monitor. It fails harmlessly when already set.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RegisterWindowClass();

    RECT frame = {0, 0, static_cast<LONG>(clientSize.width), static_cast<LONG>(clientSize.height)};
    AdjustWindowRectExForDpi(&frame, WindowedStyle, FALSE, 0, GetDpiForSystem());

    std::wstring titleText(title);
    handle = CreateWindowExW(0, WindowClassName, titleText.c_str(), WindowedStyle, CW_USEDEFAULT, CW_USEDEFAULT,
        frame.right - frame.left, frame.bottom - frame.top, nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (handle == nullptr)
        throw PlatformError(std::format("The window could not be created (Windows error {})", GetLastError()));

    ShowWindow(handle, SW_SHOW);
}

Window::~Window()
{
    listener = NobodyListening();
    SetWindowLongPtrW(handle, GWLP_USERDATA, 0);
    DestroyWindow(handle);
}

void Window::SetListener(WindowListener* newListener)
{
    listener = newListener != nullptr ? newListener : NobodyListening();
}

WindowSize Window::ClientSize() const
{
    RECT client = {};
    GetClientRect(handle, &client);
    return {static_cast<std::uint32_t>(client.right - client.left), static_cast<std::uint32_t>(client.bottom - client.top)};
}

bool Window::IsMinimized() const
{
    return IsIconic(handle) != FALSE;
}

bool Window::IsFullscreen() const
{
    return isFullscreen;
}

void Window::ToggleFullscreen()
{
    if (isFullscreen)
        LeaveFullscreen();
    else
        EnterFullscreen();

    isFullscreen = !isFullscreen;
}

bool Window::IsCloseRequested() const
{
    return closeWasRequested;
}

HWND Window::NativeHandle() const
{
    return handle;
}

void Window::RegisterWindowClass()
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = WindowClassName;

    // Registering a second time fails, which is fine: the class is already there.
    static const ATOM registeredClass = RegisterClassExW(&windowClass);
    if (registeredClass == 0)
        throw PlatformError(std::format("The window class could not be registered (Windows error {})", GetLastError()));
}

LRESULT CALLBACK Window::WindowProcedure(HWND windowHandle, UINT message, WPARAM firstParameter, LPARAM secondParameter)
{
    if (message == WM_NCCREATE)
    {
        const CREATESTRUCTW* creation = reinterpret_cast<const CREATESTRUCTW*>(secondParameter);
        Window* window = static_cast<Window*>(creation->lpCreateParams);
        window->handle = windowHandle;
        SetWindowLongPtrW(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }

    Window* window = reinterpret_cast<Window*>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA));
    if (window == nullptr)
        return DefWindowProcW(windowHandle, message, firstParameter, secondParameter);

    return window->HandleMessage(message, firstParameter, secondParameter);
}

LRESULT Window::HandleMessage(UINT message, WPARAM firstParameter, LPARAM secondParameter)
{
    switch (message)
    {
    case WM_CLOSE:
        closeWasRequested = true;
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(secondParameter) != HTCLIENT)
            break;
        SetCursor(nullptr);
        return TRUE;

    case WM_ERASEBKGND:
        return 1;

    // The Alt key alone would open the window menu and stop the program until the menu is closed.
    case WM_SYSCOMMAND:
        if ((firstParameter & SystemCommandMask) == SC_KEYMENU)
            return 0;
        break;

    case WM_DPICHANGED:
        MoveToSuggestedRectangle(*reinterpret_cast<const RECT*>(secondParameter));
        return 0;

    case WM_MOUSEMOVE:
        HandleMouseMove(secondParameter);
        return 0;

    case WM_MOUSELEAVE:
        isWaitingForMouseLeave = false;
        listener->OnMouseLeft();
        return 0;

    case WM_LBUTTONDOWN:
        listener->OnMouseButton(MouseButton::Left, true);
        return 0;

    case WM_LBUTTONUP:
        listener->OnMouseButton(MouseButton::Left, false);
        return 0;

    case WM_MBUTTONDOWN:
        listener->OnMouseButton(MouseButton::Middle, true);
        return 0;

    case WM_MBUTTONUP:
        listener->OnMouseButton(MouseButton::Middle, false);
        return 0;

    case WM_RBUTTONDOWN:
        listener->OnMouseButton(MouseButton::Right, true);
        return 0;

    case WM_RBUTTONUP:
        listener->OnMouseButton(MouseButton::Right, false);
        return 0;

    case WM_KEYDOWN:
        HandleKey(firstParameter, secondParameter, true);
        return 0;

    case WM_KEYUP:
        HandleKey(firstParameter, secondParameter, false);
        return 0;

    // Keys with the Alt key are also left to Windows, so that Alt+F4 still closes the window.
    case WM_SYSKEYDOWN:
        HandleKey(firstParameter, secondParameter, true);
        break;

    case WM_SYSKEYUP:
        HandleKey(firstParameter, secondParameter, false);
        break;

    case WM_KILLFOCUS:
        listener->OnFocusLost();
        return 0;
    }

    return DefWindowProcW(handle, message, firstParameter, secondParameter);
}

void Window::HandleMouseMove(LPARAM mousePosition)
{
    if (!isWaitingForMouseLeave)
    {
        TRACKMOUSEEVENT request = {};
        request.cbSize = sizeof(request);
        request.dwFlags = TME_LEAVE;
        request.hwndTrack = handle;
        TrackMouseEvent(&request);
        isWaitingForMouseLeave = true;
    }

    // The two halves of the parameter are signed: the position is negative while the mouse is left of or above the window.
    int x = static_cast<std::int16_t>(LOWORD(mousePosition));
    int y = static_cast<std::int16_t>(HIWORD(mousePosition));
    listener->OnMouseMoved(x, y);
}

void Window::HandleKey(WPARAM virtualKeyCode, LPARAM keyInformation, bool isPressed)
{
    bool isRepeating = isPressed && (keyInformation & KeyWasDownBit) != 0;
    if (isRepeating)
        return;

    listener->OnKey(static_cast<int>(virtualKeyCode), isPressed);
}

void Window::MoveToSuggestedRectangle(const RECT& suggestedRectangle)
{
    if (isFullscreen)
        return;

    SetWindowPos(handle, nullptr, suggestedRectangle.left, suggestedRectangle.top,
        suggestedRectangle.right - suggestedRectangle.left, suggestedRectangle.bottom - suggestedRectangle.top,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::EnterFullscreen()
{
    windowedPlacement.length = sizeof(windowedPlacement);
    GetWindowPlacement(handle, &windowedPlacement);

    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitorInfo);
    const RECT& monitor = monitorInfo.rcMonitor;

    SetWindowLongPtrW(handle, GWL_STYLE, FullscreenStyle | WS_VISIBLE);
    SetWindowPos(handle, HWND_TOP, monitor.left, monitor.top, monitor.right - monitor.left, monitor.bottom - monitor.top,
        SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
}

void Window::LeaveFullscreen()
{
    SetWindowLongPtrW(handle, GWL_STYLE, WindowedStyle | WS_VISIBLE);
    SetWindowPlacement(handle, &windowedPlacement);
    SetWindowPos(handle, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
}

}
