#include "Platform/Window.h"

#include "Platform/AspectLock.h"
#include "Platform/KeyCodes.h"
#include "Platform/PlatformError.h"

#include <cstdint>
#include <format>
#include <string>

namespace Platform
{

namespace
{

constexpr wchar_t WindowClassName[] = L"WeeOSWindow";

// The icon of the program: number 1 of its resources (source/WeeOS/WeeOS.rc).
constexpr int ProgramIconNumber = 1;

constexpr DWORD WindowedStyle = WS_OVERLAPPEDWINDOW;
constexpr DWORD FullscreenStyle = WS_POPUP;

// Windows keeps four bits of a system command for itself.
constexpr WPARAM SystemCommandMask = 0xFFF0;

// A key message has this bit set when the key was already down, which means the key is repeating.
constexpr LPARAM KeyWasDownBit = 1 << 30;

// A system key message has this bit set when the Alt key is down.
constexpr LPARAM AltIsDownBit = 1 << 29;

// A key message holds the scan code of the key in its bits 16 to 23, and bit 24 says that the key is an extended key.
constexpr int ScanCodeShift = 16;
constexpr LPARAM ScanCodeMask = 0xFF;
constexpr LPARAM ExtendedKeyBit = 1 << 24;

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

    void OnMouseWheel(int) override
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

DraggedEdge DraggedEdgeOf(WPARAM sizingEdge)
{
    switch (sizingEdge)
    {
    case WMSZ_LEFT:
        return DraggedEdge::Left;
    case WMSZ_RIGHT:
        return DraggedEdge::Right;
    case WMSZ_TOP:
        return DraggedEdge::Top;
    case WMSZ_BOTTOM:
        return DraggedEdge::Bottom;
    case WMSZ_TOPLEFT:
        return DraggedEdge::TopLeft;
    case WMSZ_TOPRIGHT:
        return DraggedEdge::TopRight;
    case WMSZ_BOTTOMLEFT:
        return DraggedEdge::BottomLeft;
    }
    return DraggedEdge::BottomRight;
}

RECT FrameAround(WindowSize clientSize, UINT dotsPerInch)
{
    RECT frame = {0, 0, static_cast<LONG>(clientSize.width), static_cast<LONG>(clientSize.height)};
    AdjustWindowRectExForDpi(&frame, WindowedStyle, FALSE, 0, dotsPerInch);
    return frame;
}

}

Window::Window(std::wstring_view title, WindowSize clientSize, WindowVisibility visibility)
    : listener(NobodyListening())
    , isShown(visibility == WindowVisibility::Shown)
{
    // Without this Windows stretches the picture of a window on a scaled monitor, so the client area would not have
    // the pixels asked for. It fails harmlessly when the program has already chosen.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RegisterWindowClass();

    RECT frame = FrameAround(clientSize, GetDpiForSystem());

    std::wstring titleText(title);
    handle = CreateWindowExW(0, WindowClassName, titleText.c_str(), WindowedStyle, CW_USEDEFAULT, CW_USEDEFAULT,
        frame.right - frame.left, frame.bottom - frame.top, nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (handle == nullptr)
        throw PlatformError(std::format("The window could not be created (Windows error {})", GetLastError()));

    if (isShown)
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

bool Window::IsShown() const
{
    return isShown;
}

WindowSize Window::ClientSize() const
{
    RECT client = {};
    GetClientRect(handle, &client);

    WindowSize size;
    size.width = static_cast<std::uint32_t>(client.right - client.left);
    size.height = static_cast<std::uint32_t>(client.bottom - client.top);
    return size;
}

bool Window::IsMinimized() const
{
    return IsIconic(handle) != FALSE;
}

void Window::SetClientSize(WindowSize clientSize)
{
    WindowPlace place = WindowedPlace();
    place.clientSize = clientSize;
    SetWindowedPlace(place);
}

// The place is the one Windows keeps for the window when it is neither minimized nor maximized, in the coordinates of the work area.
WindowPlace Window::WindowedPlace() const
{
    WINDOWPLACEMENT placement = windowedPlacement;
    if (!isFullscreen)
    {
        placement.length = sizeof(placement);
        GetWindowPlacement(handle, &placement);
    }

    const RECT& normal = placement.rcNormalPosition;
    WindowSize frame = FrameSize();

    WindowPlace place;
    place.left = normal.left;
    place.top = normal.top;
    place.clientSize.width = static_cast<std::uint32_t>(normal.right - normal.left) - frame.width;
    place.clientSize.height = static_cast<std::uint32_t>(normal.bottom - normal.top) - frame.height;
    return place;
}

void Window::SetWindowedPlace(const WindowPlace& place)
{
    RECT frame = FrameAround(place.clientSize, GetDpiForWindow(handle));
    RECT normal = {place.left, place.top, place.left + frame.right - frame.left, place.top + frame.bottom - frame.top};
    if (MonitorFromRect(&normal, MONITOR_DEFAULTTONULL) == nullptr)
        return;

    if (isFullscreen)
    {
        windowedPlacement.rcNormalPosition = normal;
        return;
    }

    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    GetWindowPlacement(handle, &placement);
    placement.rcNormalPosition = normal;
    placement.showCmd = isShown ? SW_SHOWNORMAL : SW_HIDE;
    SetWindowPlacement(handle, &placement);
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

bool Window::TakePlaceChange()
{
    bool hasChanged = placeHasChanged;
    placeHasChanged = false;
    return hasChanged;
}

bool Window::TakeCloseRequest()
{
    bool wasRequested = closeWasRequested;
    closeWasRequested = false;
    return wasRequested;
}

HWND Window::NativeHandle() const
{
    return handle;
}

void Window::RegisterWindowClass()
{
    HINSTANCE program = GetModuleHandleW(nullptr);

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = program;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(windowClass.hInstance, MAKEINTRESOURCEW(ProgramIconNumber));
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
    if (IsFullscreenKey(message, firstParameter, secondParameter))
    {
        ToggleFullscreen();
        placeHasChanged = true;
        return 0;
    }

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
        return TRUE;

    case WM_SIZING:
        KeepSixteenByNine(firstParameter, *reinterpret_cast<RECT*>(secondParameter));
        return TRUE;

    case WM_GETMINMAXINFO:
        SetSmallestSize(*reinterpret_cast<MINMAXINFO*>(secondParameter));
        return 0;

    case WM_EXITSIZEMOVE:
        placeHasChanged = true;
        return 0;

    // The Alt key alone would open the window menu and stop the program until the menu is closed.
    case WM_SYSCOMMAND:
        if ((firstParameter & SystemCommandMask) == SC_KEYMENU)
            return 0;
        break;

    // Alt+Enter would make Windows ring its bell.
    case WM_SYSCHAR:
        if (firstParameter == VK_RETURN)
            return 0;
        break;

    case WM_MOUSEMOVE:
        HandleMouseMove(secondParameter);
        return 0;

    case WM_MOUSELEAVE:
        isWaitingForMouseLeave = false;
        listener->OnMouseLeft();
        return 0;

    case WM_MOUSEWHEEL:
        HandleMouseWheel(firstParameter);
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

// F11, or Enter while Alt is down. Holding the key down switches only once.
bool Window::IsFullscreenKey(UINT message, WPARAM virtualKeyCode, LPARAM keyInformation) const
{
    bool isRepeating = (keyInformation & KeyWasDownBit) != 0;
    if (isRepeating)
        return false;

    bool isFullscreenKey = message == WM_KEYDOWN && virtualKeyCode == VK_F11;
    bool isAltEnter = message == WM_SYSKEYDOWN && virtualKeyCode == VK_RETURN && (keyInformation & AltIsDownBit) != 0;
    return isFullscreenKey || isAltEnter;
}

void Window::KeepSixteenByNine(WPARAM draggedEdge, RECT& edges) const
{
    WindowEdges dragged = {edges.left, edges.top, edges.right, edges.bottom};
    WindowEdges kept = KeepInsideSixteenByNine(dragged, DraggedEdgeOf(draggedEdge), FrameSize());
    edges = {kept.left, kept.top, kept.right, kept.bottom};
}

void Window::SetSmallestSize(MINMAXINFO& limits) const
{
    WindowSize frame = FrameSize();
    limits.ptMinTrackSize.x = static_cast<LONG>(SmallestClientSize.width + frame.width);
    limits.ptMinTrackSize.y = static_cast<LONG>(SmallestClientSize.height + frame.height);
}

// How much wider and taller the window with its frame is than the area inside, at the scale of the monitor it is on.
WindowSize Window::FrameSize() const
{
    RECT frame = FrameAround({0, 0}, GetDpiForWindow(handle));
    return {static_cast<std::uint32_t>(frame.right - frame.left), static_cast<std::uint32_t>(frame.bottom - frame.top)};
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

// A wheel with fine steps reports parts of a notch; they are added up until they make whole notches.
void Window::HandleMouseWheel(WPARAM wheelInformation)
{
    wheelTurnSinceLastNotch += GET_WHEEL_DELTA_WPARAM(wheelInformation);

    int notches = wheelTurnSinceLastNotch / WHEEL_DELTA;
    if (notches == 0)
        return;

    wheelTurnSinceLastNotch -= notches * WHEEL_DELTA;
    listener->OnMouseWheel(notches);
}

void Window::HandleKey(WPARAM virtualKeyCode, LPARAM keyInformation, bool isPressed)
{
    bool isRepeating = isPressed && (keyInformation & KeyWasDownBit) != 0;
    if (isRepeating)
        return;

    unsigned int scanCode = static_cast<unsigned int>((keyInformation >> ScanCodeShift) & ScanCodeMask);
    bool isExtendedKey = (keyInformation & ExtendedKeyBit) != 0;
    int keyCode = ReportedKeyCode(static_cast<int>(virtualKeyCode), scanCode, isExtendedKey);
    listener->OnKey(keyCode, isPressed);
}

// A window that is never shown stays hidden in every style it gets.
DWORD Window::StyleWithVisibility(DWORD style) const
{
    if (isShown)
        return style | WS_VISIBLE;

    return style;
}

void Window::EnterFullscreen()
{
    windowedPlacement.length = sizeof(windowedPlacement);
    GetWindowPlacement(handle, &windowedPlacement);

    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitorInfo);
    const RECT& monitor = monitorInfo.rcMonitor;

    SetWindowLongPtrW(handle, GWL_STYLE, StyleWithVisibility(FullscreenStyle));
    SetWindowPos(handle, HWND_TOP, monitor.left, monitor.top, monitor.right - monitor.left, monitor.bottom - monitor.top,
        SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
}

void Window::LeaveFullscreen()
{
    SetWindowLongPtrW(handle, GWL_STYLE, StyleWithVisibility(WindowedStyle));
    if (!isShown)
        windowedPlacement.showCmd = SW_HIDE;
    SetWindowPlacement(handle, &windowedPlacement);
    SetWindowPos(handle, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

}
