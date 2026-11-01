#pragma once

#include "Browser/PageCapture.h"
#include "Browser/PageReport.h"
#include "Browser/WebPage.h"

#include "WebView2.h"

#include <windows.h>
#include <wrl/client.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Browser
{

// The browser engine in the thread of the page (PageThread): the window outside the screens that the engine draws into, the
// WebView2 controller with its events, the capture of the window and the input through the DevTools protocol. Every function runs on
// that thread, which is a single-threaded COM apartment with a message loop, as WebView2 needs.
class PageEngine
{
public:
    // Starts the engine. It is ready a moment later (IsReady); then wakeThread is called so that the thread runs what waits.
    PageEngine(const BrowserSettings& settings, PageReport& report, std::function<void()> wakeThread);
    ~PageEngine();

    PageEngine(const PageEngine&) = delete;
    PageEngine& operator=(const PageEngine&) = delete;

    bool IsReady() const;

    void SetViewSize(const ViewSize& size);
    void SetRedirect(const AddressRedirect& newRedirect);
    void ServeFiles(const std::string& hostName, std::map<std::string, std::vector<std::uint8_t>> files);

    void Open(const std::string& address);
    void GoBack();
    void GoForward();
    void Reload();
    void Stop();
    void SetZoom(double zoom);

    void MovePointer(double pageX, double pageY);
    void PressPointer();
    void ReleasePointer();
    void Scroll(double pixelsRight, double pixelsDown);
    void TypeText(const std::u16string& text);
    void PressKey(PageKey key);
    void FillTextField(const std::u16string& text);
    void AnswerScriptDialog(bool isAccepted, const std::u16string& text);
    void DeleteCookies();

private:
    void MakeHostWindow();
    void PlaceHostWindow();
    void StartEnvironment(const BrowserSettings& settings);
    HRESULT OnEnvironmentMade(HRESULT result, ICoreWebView2Environment* madeEnvironment);
    HRESULT OnControllerMade(HRESULT result, ICoreWebView2Controller* madeController);
    void ChooseSettings();
    void ListenToPageEvents();
    void ListenToLoadEvents();
    void ListenToQuestions();
    void ReportStartError(const std::string& message);
    void ReportLoadStep(bool isLoading, double progress);
    void ReportFailure(const std::string& address, LoadError error);
    void ReportPicture(Formats::RgbaImage&& picture);

    bool RedirectIfNeeded(ICoreWebView2NavigationStartingEventArgs& arguments);
    void ServeRequest(ICoreWebView2WebResourceRequestedEventArgs& arguments);
    void CallDevTools(const wchar_t* method, const std::string& parameters);

    PageReport& report;
    std::function<void()> wakeThread;
    bool isMuted;
    HWND hostWindow = nullptr;
    ViewSize viewSize;
    AddressRedirect redirect;
    std::map<std::string, std::map<std::string, std::vector<std::uint8_t>>> servedHosts;
    double pointerX = 0;
    double pointerY = 0;
    std::string lastStartedAddress;

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> webView;
    Microsoft::WRL::ComPtr<ICoreWebView2ScriptDialogOpeningEventArgs> openDialog;
    Microsoft::WRL::ComPtr<ICoreWebView2Deferral> openDialogDeferral;
    std::unique_ptr<PageCapture> capture;
};

}
