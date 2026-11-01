#pragma once

#include <windows.h>

#include <filesystem>
#include <string>
#include <thread>

namespace Setup
{

enum class SetupKind
{
    Install,
    Uninstall,
};

// The window of the setup and of the uninstaller, in the calm white and blue of the Wii with the WeeOS logo: one page at a time with
// a title, a few words and up to two buttons.
class SetupWindow
{
public:
    SetupWindow(SetupKind setupKind, std::filesystem::path chosenInstallFolder);
    ~SetupWindow();

    SetupWindow(const SetupWindow&) = delete;
    SetupWindow& operator=(const SetupWindow&) = delete;

    // Shows the window until the person closes it. Returns the exit code of the program: 0 when WeeOS was installed or removed.
    int Run();

private:
    enum class Page
    {
        ChooseFolder,
        Installing,
        Installed,
        InstallFailed,
        AskToRemove,
        AskAboutTheConsole,
        Removed,
        RemoveFailed,
    };

    struct PageWords
    {
        std::wstring title;
        std::wstring text;

        // An empty label hides the button.
        std::wstring firstButton;
        std::wstring secondButton;
    };

    static LRESULT CALLBACK WindowProcedure(HWND windowHandle, UINT message, WPARAM firstParameter, LPARAM secondParameter);
    LRESULT HandleMessage(UINT message, WPARAM firstParameter, LPARAM secondParameter);

    void CreateTheWindow();
    void MakeFonts();
    void DeleteFonts();
    void LayOut();
    int Scaled(int length) const;
    RECT ScaledRectangle(const RECT& rectangle) const;
    PageWords WordsOfThePage() const;
    void ShowPage(Page newPage);

    void Paint(HDC deviceContext);
    void DrawBackground(HDC deviceContext, const RECT& area, int topInWindow);
    void DrawLogo(HDC deviceContext);
    void DrawWords(HDC deviceContext);
    void DrawFolderBox(HDC deviceContext);
    void DrawProgressBar(HDC deviceContext);
    void DrawButton(const DRAWITEMSTRUCT& drawing);
    COLORREF BackgroundColorAt(int heightInWindow) const;

    void OnButton(int buttonId);
    void PressFirstButton();
    void PressSecondButton();
    void Close();
    void ChooseFolder();
    void StartInstalling();
    void InstallInTheBackground();
    void FinishInstalling(bool succeeded);
    void Uninstall(bool removeConsole);

    SetupKind kind;
    std::filesystem::path installFolder;
    Page page;

    HWND window = nullptr;
    HWND firstButton = nullptr;
    HWND secondButton = nullptr;
    HWND changeFolderButton = nullptr;

    HFONT logoFont = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
    HFONT buttonFont = nullptr;
    int dotsPerInch = USER_DEFAULT_SCREEN_DPI;

    // How much of the installation is done, from 0 to 1.
    double progress = 0;

    std::wstring failureMessage;
    bool consoleWasKept = true;
    std::thread installer;
    int exitCode = 1;
};

}
