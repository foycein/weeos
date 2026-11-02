#pragma once

#include "Formats/RgbaImage.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Browser
{

class PageThread;

// Where the browser engine keeps its cache and cookies, and whether pages may make sound.
struct BrowserSettings
{
    std::filesystem::path dataFolder;
    bool isMuted = false;
};

// The size of the page: its width and height in pixels of the page (CSS pixels) and how many pixels of the picture one pixel of the page is.
struct ViewSize
{
    int pageWidth = 0;
    int pageHeight = 0;
    double pixelsPerPagePixel = 1;
};

// What the page shows now.
struct PageState
{
    std::string address;
    std::u16string title;
    bool isLoading = false;

    // From 0 (nothing loaded yet) to 1 (loaded). The engine tells the steps of a load, not the bytes, so the value jumps from step to step.
    double loadingProgress = 0;

    bool canGoBack = false;
    bool canGoForward = false;
};

enum class LoadError
{
    ServerNotFound,
    ConnectionFailed,
    TimedOut,
    CertificateError,
    NoInternet,

    // The address belongs to a service of Nintendo, and no server takes its place (AddressRedirect).
    ServerNotSet,

    Other,
};

struct LoadFailure
{
    std::string address;
    LoadError error = LoadError::Other;
};

// The question of a page's script: alert, confirm, prompt, or the question before a page with unsaved input is left.
struct ScriptDialog
{
    enum class Kind
    {
        Alert,
        Confirm,
        Prompt,
        LeavePage,
    };

    Kind kind = Kind::Alert;
    std::u16string message;
    std::u16string defaultText;
};

// A text field of the page was clicked: the channel shows its own keyboard for it and fills the field with FillTextField.
struct TextFieldRequest
{
    enum class Kind
    {
        Line,
        Password,
        Lines,
    };

    Kind kind = Kind::Line;
    std::u16string text;
};

// Keys that do something other than typing a character.
enum class PageKey
{
    Enter,
    Backspace,
    Delete,
    Tab,
    Escape,
    Left,
    Right,
    Up,
    Down,
    PageUp,
    PageDown,
    Home,
    End,
};

// Pages of these hosts (and of their subdomains) go to the server instead; with no server they fail with LoadError::ServerNotSet.
struct AddressRedirect
{
    std::vector<std::string> hostNames;

    // For example "https://wii.example.net". Empty while there is no server.
    std::string serverAddress;
};

// One web page shown by Chromium through WebView2, the engine of Windows 11. The engine draws the page into a
// visual of Windows' composition that no window shows; Windows' screen capture hands over a picture of that visual whenever the page
// changes, and the program draws that picture itself. Pointer, clicks, wheel and keys reach the page through the engine's DevTools
// protocol, so the page never needs the focus of Windows and nothing of the engine appears on the screen.
//
// The engine runs in a thread of its own. Every call returns at once; what the engine answers shows up in State and the Take functions.
class WebPage
{
public:
    explicit WebPage(const BrowserSettings& settings);
    ~WebPage();

    WebPage(const WebPage&) = delete;
    WebPage& operator=(const WebPage&) = delete;

    // True once the engine has started and takes pages.
    bool IsReady() const;

    // Why the engine could not start, for example because the WebView2 Runtime is missing. Nothing while it starts or runs.
    std::optional<std::string> StartError() const;

    void SetViewSize(const ViewSize& size);
    void SetRedirect(const AddressRedirect& redirect);

    // The files of a host that the program serves itself, for example the manual of a channel: path without the leading slash -> bytes.
    // An address https://<hostName>/<path> shows them.
    void ServeFiles(const std::string& hostName, std::map<std::string, std::vector<std::uint8_t>> files);

    // A script that runs in every page that opens from now on, before the page's own scripts: the functions that a channel's
    // browser gives its pages.
    void AddPageScript(const std::u16string& script);

    void Open(const std::string& address);
    void GoBack();
    void GoForward();
    void Reload();
    void Stop();

    // 1 is the page's own size.
    void SetZoom(double zoom);
    double Zoom() const;

    // In pixels of the page, from the top left corner of the view.
    void MovePointer(double pageX, double pageY);
    void PressPointer();
    void ReleasePointer();
    // Turns the wheel at the pointer: positive numbers scroll right and down.
    void Scroll(double pixelsRight, double pixelsDown);

    void TypeText(const std::u16string& text);
    void PressKey(PageKey key);

    // Puts the text into the field that TakeTextFieldRequest named.
    void FillTextField(const std::u16string& text);

    // Single Column Mode: every page from now on is laid out as one column, until it is switched off.
    void SetSingleColumn(bool isSingleColumn);

    void AnswerScriptDialog(bool isAccepted, const std::u16string& text = {});
    void DeleteCookies();

    PageState State() const;

    // The newest picture of the page, once. Nothing when the page has not changed since the last call.
    std::optional<Formats::RgbaImage> TakeNewPicture();

    std::optional<LoadFailure> TakeLoadFailure();
    std::optional<ScriptDialog> TakeScriptDialog();
    std::optional<TextFieldRequest> TakeTextFieldRequest();

private:
    std::unique_ptr<PageThread> thread;
    double zoom = 1;
};

}
