#include "Platform/ErrorMessage.h"

#include <windows.h>

#include <string>

namespace Platform
{

namespace
{

std::wstring WideText(std::string_view utf8Text)
{
    int textLength = static_cast<int>(utf8Text.size());
    int wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), textLength, nullptr, 0);

    std::wstring wideText(wideLength, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), textLength, wideText.data(), wideLength);
    return wideText;
}

}

void ShowErrorMessage(std::string_view title, std::string_view text)
{
    HWND noOwnerWindow = nullptr;
    MessageBoxW(noOwnerWindow, WideText(text).c_str(), WideText(title).c_str(), MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

}
