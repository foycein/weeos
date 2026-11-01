#include "SetupWindow.h"

#include "Installation.h"

#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <exception>
#include <iterator>

namespace Setup
{

namespace
{

using Microsoft::WRL::ComPtr;

constexpr wchar_t WindowClassName[] = L"WeeOSSetupWindow";
constexpr DWORD WindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
constexpr int ProgramIconNumber = 1;

// The calm white and blue of the Wii, as on the welcome screen of WeeOS (source/SystemMenu/WelcomeScreen.cpp).
constexpr COLORREF BackgroundTop = RGB(255, 255, 255);
constexpr COLORREF BackgroundBottom = RGB(222, 236, 247);
constexpr COLORREF WiiBlue = RGB(52, 190, 237);
constexpr COLORREF TextGray = RGB(85, 85, 85);
constexpr COLORREF QuietGray = RGB(120, 120, 120);
constexpr COLORREF FrameBlue = RGB(150, 205, 232);
constexpr COLORREF PressedFill = RGB(222, 236, 247);
constexpr COLORREF White = RGB(255, 255, 255);

constexpr wchar_t Logo[] = L"WeeOS";
constexpr wchar_t FontName[] = L"Segoe UI";

// The page, in points of a screen of 96 dots per inch.
constexpr int PageWidth = 560;
constexpr int PageHeight = 400;
constexpr RECT LogoArea = {0, 30, PageWidth, 104};
constexpr RECT LogoLineArea = {205, 108, 355, 111};
constexpr RECT TitleArea = {40, 128, 520, 160};
constexpr RECT TextArea = {50, 166, 510, 240};
constexpr RECT FolderArea = {50, 246, 400, 282};
constexpr RECT ChangeFolderButtonArea = {410, 246, 510, 282};
constexpr RECT ProgressArea = {80, 256, 480, 272};
constexpr RECT FirstButtonArea = {100, 320, 270, 364};
constexpr RECT SecondButtonArea = {290, 320, 460, 364};
constexpr RECT OnlyButtonArea = {195, 320, 365, 364};

constexpr int LogoSize = 64;
constexpr int TitleSize = 22;
constexpr int TextSize = 15;
constexpr int ButtonTextSize = 16;
constexpr int FolderCornerSize = 14;
constexpr int FrameWidth = 2;
constexpr int FocusFrameWidth = 3;
constexpr int TextMargin = 10;

constexpr int FirstButtonId = 1001;
constexpr int SecondButtonId = 1002;
constexpr int ChangeFolderButtonId = 1003;

// The installer thread reports to the window with these messages: the progress in thousandths, and the end (1 when it succeeded).
constexpr UINT ProgressMessage = WM_APP + 1;
constexpr UINT InstalledMessage = WM_APP + 2;
constexpr int ProgressSteps = 1000;

COLORREF Mix(COLORREF firstColor, COLORREF secondColor, double shareOfSecond)
{
    auto mixPart = [shareOfSecond](int first, int second) { return static_cast<int>(first + (second - first) * shareOfSecond + 0.5); };
    int red = mixPart(GetRValue(firstColor), GetRValue(secondColor));
    int green = mixPart(GetGValue(firstColor), GetGValue(secondColor));
    int blue = mixPart(GetBValue(firstColor), GetBValue(secondColor));
    return RGB(red, green, blue);
}

TRIVERTEX Corner(int x, int y, COLORREF color)
{
    // A corner of GradientFill takes 16 bits for each part of the color.
    constexpr int ColorShift = 8;

    TRIVERTEX corner = {};
    corner.x = x;
    corner.y = y;
    corner.Red = static_cast<COLOR16>(GetRValue(color) << ColorShift);
    corner.Green = static_cast<COLOR16>(GetGValue(color) << ColorShift);
    corner.Blue = static_cast<COLOR16>(GetBValue(color) << ColorShift);
    return corner;
}

void FillGradient(HDC deviceContext, const RECT& area, COLORREF startColor, COLORREF endColor, ULONG direction)
{
    TRIVERTEX corners[] = {Corner(area.left, area.top, startColor), Corner(area.right, area.bottom, endColor)};
    GRADIENT_RECT rectangle = {0, 1};
    GradientFill(deviceContext, corners, 2, &rectangle, 1, direction);
}

void DrawRoundedBox(HDC deviceContext, const RECT& area, int cornerSize, COLORREF fill, COLORREF frame, int frameWidth)
{
    HPEN pen = CreatePen(PS_INSIDEFRAME, frameWidth, frame);
    HBRUSH brush = CreateSolidBrush(fill);
    HGDIOBJ oldPen = SelectObject(deviceContext, pen);
    HGDIOBJ oldBrush = SelectObject(deviceContext, brush);

    RoundRect(deviceContext, area.left, area.top, area.right, area.bottom, cornerSize, cornerSize);

    SelectObject(deviceContext, oldPen);
    SelectObject(deviceContext, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawTextIn(HDC deviceContext, const std::wstring& text, RECT area, HFONT font, COLORREF color, UINT format)
{
    HGDIOBJ oldFont = SelectObject(deviceContext, font);
    SetTextColor(deviceContext, color);
    SetBkMode(deviceContext, TRANSPARENT);
    DrawTextW(deviceContext, text.c_str(), static_cast<int>(text.size()), &area, format | DT_NOPREFIX);
    SelectObject(deviceContext, oldFont);
}

std::wstring WideText(const char* text)
{
    int length = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    std::wstring wideText(length, L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, wideText.data(), length);
    wideText.resize(length - 1);
    return wideText;
}

RECT Shrunk(const RECT& area, int margin)
{
    return {area.left + margin, area.top + margin, area.right - margin, area.bottom - margin};
}

}

SetupWindow::SetupWindow(SetupKind kind, std::filesystem::path installFolder)
    : kind(kind), installFolder(std::move(installFolder))
{
    page = kind == SetupKind::Install ? Page::ChooseFolder : Page::AskToRemove;
    CreateTheWindow();
}

SetupWindow::~SetupWindow()
{
    if (installer.joinable())
        installer.join();

    DeleteFonts();
}

int SetupWindow::Run()
{
    ShowWindow(window, SW_SHOWNORMAL);
    SetFocus(firstButton);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        // Gives the keys of a dialog: Tab between the buttons, Enter, Escape and Space.
        if (IsDialogMessageW(window, &message))
            continue;

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return exitCode;
}

void SetupWindow::CreateTheWindow()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(ProgramIconNumber));
    windowClass.lpszClassName = WindowClassName;
    RegisterClassExW(&windowClass);

    std::wstring title = kind == SetupKind::Install ? L"WeeOS Setup" : L"Uninstall WeeOS";
    window = CreateWindowExW(0, WindowClassName, title.c_str(), WindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, PageWidth, PageHeight, nullptr,
        nullptr, instance, this);

    dotsPerInch = static_cast<int>(GetDpiForWindow(window));
    MakeFonts();

    auto makeButton = [this, instance](int buttonId) {
        DWORD style = WS_CHILD | WS_TABSTOP | BS_OWNERDRAW;
        return CreateWindowExW(0, L"BUTTON", L"", style, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(buttonId)),
            instance, nullptr);
    };
    firstButton = makeButton(FirstButtonId);
    secondButton = makeButton(SecondButtonId);
    changeFolderButton = makeButton(ChangeFolderButtonId);
    SetWindowTextW(changeFolderButton, L"Change...");

    // The window gets the size of the page, in the middle of the screen it is on.
    RECT windowArea = {0, 0, Scaled(PageWidth), Scaled(PageHeight)};
    AdjustWindowRectExForDpi(&windowArea, WindowStyle, FALSE, 0, static_cast<UINT>(dotsPerInch));
    int windowWidth = windowArea.right - windowArea.left;
    int windowHeight = windowArea.bottom - windowArea.top;

    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitor);
    int left = (monitor.rcWork.left + monitor.rcWork.right - windowWidth) / 2;
    int top = (monitor.rcWork.top + monitor.rcWork.bottom - windowHeight) / 2;
    SetWindowPos(window, nullptr, left, top, windowWidth, windowHeight, SWP_NOZORDER | SWP_NOACTIVATE);

    ShowPage(page);
}

void SetupWindow::MakeFonts()
{
    DeleteFonts();

    auto makeFont = [this](int size, int weight) {
        return CreateFontW(-Scaled(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, FontName);
    };
    logoFont = makeFont(LogoSize, FW_SEMIBOLD);
    titleFont = makeFont(TitleSize, FW_SEMIBOLD);
    textFont = makeFont(TextSize, FW_NORMAL);
    buttonFont = makeFont(ButtonTextSize, FW_NORMAL);
}

void SetupWindow::DeleteFonts()
{
    for (HFONT font : {logoFont, titleFont, textFont, buttonFont})
    {
        if (font != nullptr)
            DeleteObject(font);
    }
}

int SetupWindow::Scaled(int length) const
{
    return MulDiv(length, dotsPerInch, USER_DEFAULT_SCREEN_DPI);
}

RECT SetupWindow::ScaledRectangle(const RECT& rectangle) const
{
    return {Scaled(rectangle.left), Scaled(rectangle.top), Scaled(rectangle.right), Scaled(rectangle.bottom)};
}

void SetupWindow::LayOut()
{
    PageWords words = WordsOfThePage();
    bool hasFirstButton = !words.firstButton.empty();
    bool hasSecondButton = !words.secondButton.empty();

    auto place = [this](HWND button, const RECT& area, bool isShown) {
        RECT scaledArea = ScaledRectangle(area);
        int width = scaledArea.right - scaledArea.left;
        int height = scaledArea.bottom - scaledArea.top;
        SetWindowPos(button, nullptr, scaledArea.left, scaledArea.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(button, isShown ? SW_SHOW : SW_HIDE);
    };

    place(firstButton, hasSecondButton ? FirstButtonArea : OnlyButtonArea, hasFirstButton);
    place(secondButton, SecondButtonArea, hasSecondButton);
    place(changeFolderButton, ChangeFolderButtonArea, page == Page::ChooseFolder);
    InvalidateRect(window, nullptr, FALSE);
}

SetupWindow::PageWords SetupWindow::WordsOfThePage() const
{
    switch (page)
    {
    case Page::ChooseFolder:
        return {L"Install WeeOS " + WeeOSVersionText(),
            L"A hand-written reconstruction of the Wii system for the PC.\nWeeOS will be installed in this folder:", L"Install", L"Cancel"};
    case Page::Installing:
        return {L"Installing WeeOS...", L"Unpacking the system and the channels.", L"", L""};
    case Page::Installed:
        return {L"WeeOS is ready",
            L"Its shortcuts are on the desktop and in the Start menu.\nThe first start makes a new Wii console of its own.", L"Start WeeOS",
            L"Close"};
    case Page::InstallFailed:
        return {L"WeeOS could not be installed", failureMessage, L"Close", L""};
    case Page::AskToRemove:
        return {L"Remove WeeOS?", L"WeeOS will be removed from this PC: the program, its shortcuts and its entry in the apps of Windows.",
            L"Remove", L"Cancel"};
    case Page::AskAboutTheConsole:
        return {L"Keep your Wii?",
            L"The folder also holds your Wii: the NAND with its Miis, saves and channels, its identity and the SD card folder. "
            L"Keep them for a later WeeOS, or remove them too?",
            L"Keep my Wii", L"Remove them too"};
    case Page::Removed:
        if (consoleWasKept)
            return {L"WeeOS was removed", L"Your Wii stays in " + installFolder.wstring() + L".\nInstall WeeOS there again to go on with it.",
                L"Close", L""};
        return {L"WeeOS was removed", L"WeeOS and your Wii are gone from this PC.", L"Close", L""};
    case Page::RemoveFailed:
        return {L"WeeOS could not be removed", failureMessage, L"Close", L""};
    }
    return {};
}

void SetupWindow::ShowPage(Page newPage)
{
    page = newPage;
    PageWords words = WordsOfThePage();
    SetWindowTextW(firstButton, words.firstButton.c_str());
    SetWindowTextW(secondButton, words.secondButton.c_str());
    LayOut();

    if (!words.firstButton.empty())
        SetFocus(firstButton);
}

LRESULT CALLBACK SetupWindow::WindowProcedure(HWND windowHandle, UINT message, WPARAM firstParameter, LPARAM secondParameter)
{
    if (message == WM_NCCREATE)
    {
        const CREATESTRUCTW* creation = reinterpret_cast<const CREATESTRUCTW*>(secondParameter);
        auto* setupWindow = static_cast<SetupWindow*>(creation->lpCreateParams);
        setupWindow->window = windowHandle;
        SetWindowLongPtrW(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(setupWindow));
    }

    auto* setupWindow = reinterpret_cast<SetupWindow*>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA));
    if (setupWindow == nullptr)
        return DefWindowProcW(windowHandle, message, firstParameter, secondParameter);

    return setupWindow->HandleMessage(message, firstParameter, secondParameter);
}

LRESULT SetupWindow::HandleMessage(UINT message, WPARAM firstParameter, LPARAM secondParameter)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT painting;
        HDC deviceContext = BeginPaint(window, &painting);
        Paint(deviceContext);
        EndPaint(window, &painting);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DRAWITEM:
        DrawButton(*reinterpret_cast<const DRAWITEMSTRUCT*>(secondParameter));
        return TRUE;
    case WM_COMMAND:
        OnButton(LOWORD(firstParameter));
        return 0;
    case WM_DPICHANGED:
    {
        dotsPerInch = HIWORD(firstParameter);
        MakeFonts();
        const RECT* suggested = reinterpret_cast<const RECT*>(secondParameter);
        SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        LayOut();
        return 0;
    }
    case ProgressMessage:
        progress = static_cast<double>(firstParameter) / ProgressSteps;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case InstalledMessage:
        FinishInstalling(firstParameter != 0);
        return 0;
    case WM_CLOSE:
        Close();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, firstParameter, secondParameter);
}

void SetupWindow::OnButton(int buttonId)
{
    // Enter and Escape come from IsDialogMessage as IDOK and IDCANCEL.
    if (buttonId == FirstButtonId || buttonId == IDOK)
        PressFirstButton();
    else if (buttonId == SecondButtonId)
        PressSecondButton();
    else if (buttonId == ChangeFolderButtonId)
        ChooseFolder();
    else if (buttonId == IDCANCEL)
        Close();
}

void SetupWindow::PressFirstButton()
{
    switch (page)
    {
    case Page::ChooseFolder:
        StartInstalling();
        return;
    case Page::Installed:
        StartWeeOS(installFolder);
        Close();
        return;
    case Page::AskToRemove:
        ShowPage(Page::AskAboutTheConsole);
        return;
    case Page::AskAboutTheConsole:
        Uninstall(false);
        return;
    case Page::InstallFailed:
    case Page::Removed:
    case Page::RemoveFailed:
        Close();
        return;
    case Page::Installing:
        return;
    }
}

void SetupWindow::PressSecondButton()
{
    if (page == Page::AskAboutTheConsole)
    {
        Uninstall(true);
        return;
    }
    Close();
}

void SetupWindow::Close()
{
    if (page == Page::Installing)
        return;

    DestroyWindow(window);
}

void SetupWindow::ChooseFolder()
{
    ComPtr<IFileOpenDialog> picker;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&picker))))
        return;

    picker->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    picker->SetTitle(L"Where should WeeOS go?");
    if (FAILED(picker->Show(window)))
        return;

    ComPtr<IShellItem> chosenItem;
    if (FAILED(picker->GetResult(&chosenItem)))
        return;

    PWSTR chosenPath = nullptr;
    if (FAILED(chosenItem->GetDisplayName(SIGDN_FILESYSPATH, &chosenPath)))
        return;

    installFolder = InstallFolderInside(chosenPath);
    CoTaskMemFree(chosenPath);
    InvalidateRect(window, nullptr, FALSE);
}

void SetupWindow::StartInstalling()
{
    progress = 0;
    ShowPage(Page::Installing);
    installer = std::thread(&SetupWindow::InstallInTheBackground, this);
}

void SetupWindow::InstallInTheBackground()
{
    HWND windowToTell = window;
    auto reportProgress = [windowToTell](double doneShare) {
        PostMessageW(windowToTell, ProgressMessage, static_cast<WPARAM>(doneShare * ProgressSteps), 0);
    };

    try
    {
        InstallWeeOS(installFolder, reportProgress);
        PostMessageW(windowToTell, InstalledMessage, TRUE, 0);
    }
    catch (const std::exception& error)
    {
        failureMessage = WideText(error.what());
        PostMessageW(windowToTell, InstalledMessage, FALSE, 0);
    }
}

void SetupWindow::FinishInstalling(bool succeeded)
{
    installer.join();
    exitCode = succeeded ? 0 : 1;
    ShowPage(succeeded ? Page::Installed : Page::InstallFailed);
}

void SetupWindow::Uninstall(bool removeConsole)
{
    try
    {
        UninstallWeeOS(removeConsole);
        consoleWasKept = !removeConsole;
        exitCode = 0;
        ShowPage(Page::Removed);
    }
    catch (const std::exception& error)
    {
        failureMessage = WideText(error.what());
        ShowPage(Page::RemoveFailed);
    }
}

COLORREF SetupWindow::BackgroundColorAt(int heightInWindow) const
{
    double share = static_cast<double>(heightInWindow) / Scaled(PageHeight);
    return Mix(BackgroundTop, BackgroundBottom, std::clamp(share, 0.0, 1.0));
}

void SetupWindow::DrawBackground(HDC deviceContext, const RECT& area, int topInWindow)
{
    int bottomInWindow = topInWindow + (area.bottom - area.top);
    FillGradient(deviceContext, area, BackgroundColorAt(topInWindow), BackgroundColorAt(bottomInWindow), GRADIENT_FILL_RECT_V);
}

// Everything is drawn into a picture first and then shown at once, so nothing flickers while the progress moves.
void SetupWindow::Paint(HDC deviceContext)
{
    RECT clientArea;
    GetClientRect(window, &clientArea);

    HDC pictureContext = CreateCompatibleDC(deviceContext);
    HBITMAP picture = CreateCompatibleBitmap(deviceContext, clientArea.right, clientArea.bottom);
    HGDIOBJ oldPicture = SelectObject(pictureContext, picture);

    DrawBackground(pictureContext, clientArea, 0);
    DrawLogo(pictureContext);
    DrawWords(pictureContext);
    if (page == Page::ChooseFolder)
        DrawFolderBox(pictureContext);
    if (page == Page::Installing)
        DrawProgressBar(pictureContext);

    BitBlt(deviceContext, 0, 0, clientArea.right, clientArea.bottom, pictureContext, 0, 0, SRCCOPY);
    SelectObject(pictureContext, oldPicture);
    DeleteObject(picture);
    DeleteDC(pictureContext);
}

// "WeeOS" in the blue of the Wii, with a short line under it that fades out towards both of its ends.
void SetupWindow::DrawLogo(HDC deviceContext)
{
    DrawTextIn(deviceContext, Logo, ScaledRectangle(LogoArea), logoFont, WiiBlue, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT line = ScaledRectangle(LogoLineArea);
    COLORREF background = BackgroundColorAt(line.top);
    int middle = (line.left + line.right) / 2;
    FillGradient(deviceContext, {line.left, line.top, middle, line.bottom}, background, WiiBlue, GRADIENT_FILL_RECT_H);
    FillGradient(deviceContext, {middle, line.top, line.right, line.bottom}, WiiBlue, background, GRADIENT_FILL_RECT_H);
}

void SetupWindow::DrawWords(HDC deviceContext)
{
    PageWords words = WordsOfThePage();
    DrawTextIn(deviceContext, words.title, ScaledRectangle(TitleArea), titleFont, TextGray, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    DrawTextIn(deviceContext, words.text, ScaledRectangle(TextArea), textFont, QuietGray, DT_CENTER | DT_WORDBREAK);
}

void SetupWindow::DrawFolderBox(HDC deviceContext)
{
    RECT box = ScaledRectangle(FolderArea);
    DrawRoundedBox(deviceContext, box, Scaled(FolderCornerSize), White, FrameBlue, Scaled(FrameWidth));

    RECT textArea = Shrunk(box, Scaled(TextMargin));
    DrawTextIn(deviceContext, installFolder.wstring(), textArea, textFont, TextGray, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
}

void SetupWindow::DrawProgressBar(HDC deviceContext)
{
    RECT track = ScaledRectangle(ProgressArea);
    int height = track.bottom - track.top;
    DrawRoundedBox(deviceContext, track, height, White, FrameBlue, Scaled(FrameWidth / 2));

    int filledWidth = static_cast<int>((track.right - track.left) * progress);
    if (filledWidth < height)
        return;

    RECT filled = {track.left, track.top, track.left + filledWidth, track.bottom};
    DrawRoundedBox(deviceContext, filled, height, WiiBlue, WiiBlue, 1);
}

// The buttons of the Wii: a white pill with a light blue frame and gray words. The button that has the keyboard gets a stronger frame.
void SetupWindow::DrawButton(const DRAWITEMSTRUCT& drawing)
{
    HDC deviceContext = drawing.hDC;
    RECT area = drawing.rcItem;

    POINT topInWindow = {0, 0};
    MapWindowPoints(drawing.hwndItem, window, &topInWindow, 1);
    DrawBackground(deviceContext, area, topInWindow.y);

    bool isPressed = (drawing.itemState & ODS_SELECTED) != 0;
    bool hasFocus = (drawing.itemState & ODS_FOCUS) != 0;
    COLORREF fill = isPressed ? PressedFill : White;
    COLORREF frame = hasFocus ? WiiBlue : FrameBlue;
    int frameWidth = Scaled(hasFocus ? FocusFrameWidth : FrameWidth);
    int height = area.bottom - area.top;
    DrawRoundedBox(deviceContext, area, height, fill, frame, frameWidth);

    wchar_t label[64] = {};
    GetWindowTextW(drawing.hwndItem, label, static_cast<int>(std::size(label)));
    DrawTextIn(deviceContext, label, area, buttonFont, TextGray, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

}
