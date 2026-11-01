#include "Browser/PageEngine.h"

#include "Browser/Addresses.h"
#include "Browser/BrowserError.h"
#include "Browser/DevToolsMessages.h"
#include "Browser/TextConversion.h"

#include <DispatcherQueue.h>

#include "WebView2EnvironmentOptions.h"

#include <shlwapi.h>
#include <wrl/event.h>

#include <algorithm>
#include <format>

namespace Browser
{

namespace
{

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

constexpr wchar_t HostWindowClass[] = L"WeeOSWebPage";


// The steps of a load, as parts of the whole (PageState::loadingProgress, Assumed: the engine does not count bytes).
constexpr double ProgressWhenStarted = 0.1;
constexpr double ProgressWhenContentComes = 0.4;
constexpr double ProgressWhenDocumentIsRead = 0.7;
constexpr double ProgressWhenLoaded = 1;

constexpr wchar_t EnglishLanguage[] = L"en-US";

// The engine stops drawing a window that Windows calls hidden behind other windows; the window of the page must always be drawn.
constexpr wchar_t BrowserArguments[] = L"--disable-features=CalculateNativeWinOcclusion";

constexpr std::string_view SecureScheme = "https://";

std::string TakeWideText(LPWSTR text)
{
    if (text == nullptr)
        return {};

    std::string utf8Text = Utf8TextOf(text);
    CoTaskMemFree(text);
    return utf8Text;
}

std::u16string TakeText(LPWSTR text)
{
    if (text == nullptr)
        return {};

    std::u16string converted = TextOf(text);
    CoTaskMemFree(text);
    return converted;
}

LoadError LoadErrorOf(COREWEBVIEW2_WEB_ERROR_STATUS status)
{
    switch (status)
    {
    case COREWEBVIEW2_WEB_ERROR_STATUS_HOST_NAME_NOT_RESOLVED:
        return LoadError::ServerNotFound;
    case COREWEBVIEW2_WEB_ERROR_STATUS_CANNOT_CONNECT:
    case COREWEBVIEW2_WEB_ERROR_STATUS_CONNECTION_ABORTED:
    case COREWEBVIEW2_WEB_ERROR_STATUS_CONNECTION_RESET:
    case COREWEBVIEW2_WEB_ERROR_STATUS_SERVER_UNREACHABLE:
        return LoadError::ConnectionFailed;
    case COREWEBVIEW2_WEB_ERROR_STATUS_TIMEOUT:
        return LoadError::TimedOut;
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_COMMON_NAME_IS_INCORRECT:
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_EXPIRED:
    case COREWEBVIEW2_WEB_ERROR_STATUS_CLIENT_CERTIFICATE_CONTAINS_ERRORS:
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_REVOKED:
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_IS_INVALID:
        return LoadError::CertificateError;
    case COREWEBVIEW2_WEB_ERROR_STATUS_DISCONNECTED:
        return LoadError::NoInternet;
    default:
        return LoadError::Other;
    }
}

ScriptDialog::Kind DialogKindOf(COREWEBVIEW2_SCRIPT_DIALOG_KIND kind)
{
    switch (kind)
    {
    case COREWEBVIEW2_SCRIPT_DIALOG_KIND_CONFIRM:
        return ScriptDialog::Kind::Confirm;
    case COREWEBVIEW2_SCRIPT_DIALOG_KIND_PROMPT:
        return ScriptDialog::Kind::Prompt;
    case COREWEBVIEW2_SCRIPT_DIALOG_KIND_BEFOREUNLOAD:
        return ScriptDialog::Kind::LeavePage;
    default:
        return ScriptDialog::Kind::Alert;
    }
}

std::wstring ContentTypeOf(const std::string& path)
{
    std::size_t dot = path.rfind('.');
    std::string extension = dot == std::string::npos ? std::string() : path.substr(dot + 1);
    if (extension == "html" || extension == "htm")
        return L"Content-Type: text/html";
    if (extension == "css")
        return L"Content-Type: text/css";
    if (extension == "js")
        return L"Content-Type: text/javascript";
    if (extension == "png")
        return L"Content-Type: image/png";
    if (extension == "gif")
        return L"Content-Type: image/gif";
    if (extension == "jpg" || extension == "jpeg")
        return L"Content-Type: image/jpeg";
    return L"Content-Type: application/octet-stream";
}

LRESULT CALLBACK HostWindowProcedure(HWND window, UINT message, WPARAM wordParameter, LPARAM longParameter)
{
    return DefWindowProcW(window, message, wordParameter, longParameter);
}

void RegisterHostWindowClass()
{
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = HostWindowProcedure;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = HostWindowClass;

    // A second page finds the class registered already, which is no error.
    RegisterClassW(&windowClass);
}

}

PageEngine::PageEngine(const BrowserSettings& settings, PageReport& pageReport, std::function<void()> wakeThreadUp)
    : report(pageReport)
    , wakeThread(std::move(wakeThreadUp))
    , isMuted(settings.isMuted)
{
    MakeHostWindow();
    MakeVisual();
    StartEnvironment(settings);
}

PageEngine::~PageEngine()
{
    capture.reset();
    if (openDialogDeferral)
        openDialogDeferral->Complete();
    if (controller)
        controller->Close();
    if (hostWindow != nullptr)
        DestroyWindow(hostWindow);
}

bool PageEngine::IsReady() const
{
    return webView != nullptr;
}

void PageEngine::MakeHostWindow()
{
    RegisterHostWindowClass();

    DWORD style = WS_POPUP;
    DWORD extendedStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    hostWindow = CreateWindowExW(extendedStyle, HostWindowClass, L"WeeOS web page", style, 0, 0, 1, 1, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (hostWindow == nullptr)
        throw BrowserError("The window of the web page could not be made");

    PlaceHostWindow();
}

// The window has the size of the page's picture. It is never shown: the engine draws into the visual.
void PageEngine::PlaceHostWindow()
{
    int width = std::max(1, static_cast<int>(viewSize.pageWidth * viewSize.pixelsPerPagePixel + 0.5));
    int height = std::max(1, static_cast<int>(viewSize.pageHeight * viewSize.pixelsPerPagePixel + 0.5));
    SetWindowPos(hostWindow, HWND_BOTTOM, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOMOVE | SWP_HIDEWINDOW);
}

// The composition of Windows needs a dispatcher queue on the thread; the thread's message loop runs it.
void PageEngine::MakeVisual()
{
    DispatcherQueueOptions options = {sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT, DQTAT_COM_STA};
    ABI::Windows::System::IDispatcherQueueController* madeController = nullptr;
    if (FAILED(CreateDispatcherQueueController(options, &madeController)))
        throw BrowserError("The composition of Windows cannot run in the thread of the web page");

    winrt::attach_abi(queueController, madeController);
    compositor = winrt::Windows::UI::Composition::Compositor();
    pageVisual = compositor.CreateContainerVisual();
}

void PageEngine::StartEnvironment(const BrowserSettings& settings)
{
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    options->put_Language(EnglishLanguage);
    options->put_AdditionalBrowserArguments(BrowserArguments);

    std::wstring dataFolder = settings.dataFolder.wstring();
    auto whenMade = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [this](HRESULT result, ICoreWebView2Environment* madeEnvironment) { return OnEnvironmentMade(result, madeEnvironment); });

    HRESULT result = CreateCoreWebView2EnvironmentWithOptions(nullptr, dataFolder.c_str(), options.Get(), whenMade.Get());
    if (FAILED(result))
        ReportStartError(std::format("The WebView2 Runtime of Windows could not be started (error {:08X})", static_cast<unsigned>(result)));
}

HRESULT PageEngine::OnEnvironmentMade(HRESULT result, ICoreWebView2Environment* madeEnvironment)
{
    if (FAILED(result) || madeEnvironment == nullptr)
    {
        ReportStartError(std::format("The WebView2 Runtime of Windows did not start (error {:08X})", static_cast<unsigned>(result)));
        return S_OK;
    }

    environment = madeEnvironment;
    ComPtr<ICoreWebView2Environment3> compositionEnvironment;
    if (FAILED(environment.As(&compositionEnvironment)))
    {
        ReportStartError("The WebView2 Runtime of Windows is too old to draw into a visual");
        return S_OK;
    }

    auto whenMade = Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
        [this](HRESULT controllerResult, ICoreWebView2CompositionController* madeController) {
            return OnControllerMade(controllerResult, madeController);
        });
    compositionEnvironment->CreateCoreWebView2CompositionController(hostWindow, whenMade.Get());
    return S_OK;
}

HRESULT PageEngine::OnControllerMade(HRESULT result, ICoreWebView2CompositionController* madeController)
{
    if (FAILED(result) || madeController == nullptr)
    {
        ReportStartError(std::format("The WebView2 Runtime of Windows made no view (error {:08X})", static_cast<unsigned>(result)));
        return S_OK;
    }

    compositionController = madeController;
    compositionController.As(&controller);
    controller->get_CoreWebView2(&webView);
    compositionController->put_RootVisualTarget(winrt::get_unknown(pageVisual));

    ChooseSettings();
    ListenToPageEvents();
    ListenToLoadEvents();
    ListenToQuestions();
    SetViewSize(viewSize);
    controller->put_IsVisible(TRUE);
    CallDevTools(L"Emulation.setFocusEmulationEnabled", FocusEmulationParameters());
    if (!capture)
        return S_OK;

    {
        std::lock_guard<std::mutex> guard(report.lock);
        report.isReady = true;
    }
    wakeThread();
    return S_OK;
}

// The engine is a page of the channel and nothing else: no menus, keys or dialogs of Edge, no saved passwords or form entries.
void PageEngine::ChooseSettings()
{
    ComPtr<ICoreWebView2Settings> settings;
    webView->get_Settings(&settings);
    settings->put_AreDefaultContextMenusEnabled(FALSE);
    settings->put_AreDefaultScriptDialogsEnabled(FALSE);
    settings->put_AreDevToolsEnabled(FALSE);
    settings->put_IsStatusBarEnabled(FALSE);
    settings->put_IsZoomControlEnabled(FALSE);

    ComPtr<ICoreWebView2Settings3> acceleratorSettings;
    if (SUCCEEDED(settings.As(&acceleratorSettings)))
        acceleratorSettings->put_AreBrowserAcceleratorKeysEnabled(FALSE);

    ComPtr<ICoreWebView2Settings4> formSettings;
    if (SUCCEEDED(settings.As(&formSettings)))
    {
        formSettings->put_IsGeneralAutofillEnabled(FALSE);
        formSettings->put_IsPasswordAutosaveEnabled(FALSE);
    }

    ComPtr<ICoreWebView2Settings5> pinchSettings;
    if (SUCCEEDED(settings.As(&pinchSettings)))
        pinchSettings->put_IsPinchZoomEnabled(FALSE);

    ComPtr<ICoreWebView2Settings6> swipeSettings;
    if (SUCCEEDED(settings.As(&swipeSettings)))
        swipeSettings->put_IsSwipeNavigationEnabled(FALSE);

    ComPtr<ICoreWebView2_8> soundControl;
    if (isMuted && SUCCEEDED(webView.As(&soundControl)))
        soundControl->put_IsMuted(TRUE);

    ComPtr<ICoreWebView2Controller3> scaleControl;
    if (SUCCEEDED(controller.As(&scaleControl)))
        scaleControl->put_ShouldDetectMonitorScaleChanges(FALSE);

    webView->AddScriptToExecuteOnDocumentCreated(WideTextOf(PageHelperScript()).c_str(), nullptr);
}

// The address, the title and the history of the page; new windows, text fields and the files the program serves.
void PageEngine::ListenToPageEvents()
{
    EventRegistrationToken token;
    webView->add_SourceChanged(Callback<ICoreWebView2SourceChangedEventHandler>([this](ICoreWebView2*, ICoreWebView2SourceChangedEventArgs*) {
        LPWSTR source = nullptr;
        webView->get_Source(&source);
        std::lock_guard<std::mutex> guard(report.lock);
        report.state.address = TakeWideText(source);
        return S_OK;
    }).Get(), &token);

    webView->add_DocumentTitleChanged(Callback<ICoreWebView2DocumentTitleChangedEventHandler>([this](ICoreWebView2*, IUnknown*) {
        LPWSTR title = nullptr;
        webView->get_DocumentTitle(&title);
        std::lock_guard<std::mutex> guard(report.lock);
        report.state.title = TakeText(title);
        return S_OK;
    }).Get(), &token);

    webView->add_HistoryChanged(Callback<ICoreWebView2HistoryChangedEventHandler>([this](ICoreWebView2*, IUnknown*) {
        BOOL canGoBack = FALSE;
        BOOL canGoForward = FALSE;
        webView->get_CanGoBack(&canGoBack);
        webView->get_CanGoForward(&canGoForward);
        std::lock_guard<std::mutex> guard(report.lock);
        report.state.canGoBack = canGoBack == TRUE;
        report.state.canGoForward = canGoForward == TRUE;
        return S_OK;
    }).Get(), &token);

    // The channel has one page, as the original: a link that asks for a new window opens in it.
    webView->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* arguments) {
            LPWSTR address = nullptr;
            arguments->get_Uri(&address);
            arguments->put_Handled(TRUE);
            Open(TakeWideText(address));
            return S_OK;
        }).Get(), &token);

    webView->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* arguments) {
            LPWSTR message = nullptr;
            if (FAILED(arguments->TryGetWebMessageAsString(&message)))
                return S_OK;

            std::optional<TextFieldRequest> request = ReadTextFieldMessage(TakeText(message));
            std::lock_guard<std::mutex> guard(report.lock);
            if (request)
                report.textField = request;
            return S_OK;
        }).Get(), &token);

    webView->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* arguments) {
            ServeRequest(*arguments);
            return S_OK;
        }).Get(), &token);
}

void PageEngine::ListenToLoadEvents()
{
    EventRegistrationToken token;
    webView->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* arguments) {
            if (!RedirectIfNeeded(*arguments))
                ReportLoadStep(true, ProgressWhenStarted);
            return S_OK;
        }).Get(), &token);

    webView->add_ContentLoading(Callback<ICoreWebView2ContentLoadingEventHandler>([this](ICoreWebView2*, ICoreWebView2ContentLoadingEventArgs*) {
        ReportLoadStep(true, ProgressWhenContentComes);
        return S_OK;
    }).Get(), &token);

    ComPtr<ICoreWebView2_2> documentEvents;
    if (SUCCEEDED(webView.As(&documentEvents)))
    {
        documentEvents->add_DOMContentLoaded(Callback<ICoreWebView2DOMContentLoadedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2DOMContentLoadedEventArgs*) {
                ReportLoadStep(true, ProgressWhenDocumentIsRead);
                if (isSingleColumn)
                    webView->ExecuteScript(WideTextOf(SingleColumnScript(true)).c_str(), nullptr);
                return S_OK;
            }).Get(), &token);
    }

    webView->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* arguments) {
            ReportLoadStep(false, ProgressWhenLoaded);

            BOOL isSuccess = TRUE;
            COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
            arguments->get_IsSuccess(&isSuccess);
            arguments->get_WebErrorStatus(&status);

            // A load that was stopped, or replaced by the next one, is no failure.
            bool wasStopped = status == COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED;
            if (!isSuccess && !wasStopped)
                ReportFailure(lastStartedAddress, LoadErrorOf(status));
            return S_OK;
        }).Get(), &token);
}

// Script dialogs come to the channel; Edge's own questions (permissions, downloads, logins, certificates) are answered with no.
void PageEngine::ListenToQuestions()
{
    EventRegistrationToken token;
    webView->add_ScriptDialogOpening(Callback<ICoreWebView2ScriptDialogOpeningEventHandler>(
        [this](ICoreWebView2*, ICoreWebView2ScriptDialogOpeningEventArgs* arguments) {
            COREWEBVIEW2_SCRIPT_DIALOG_KIND kind = COREWEBVIEW2_SCRIPT_DIALOG_KIND_ALERT;
            LPWSTR message = nullptr;
            LPWSTR defaultText = nullptr;
            arguments->get_Kind(&kind);
            arguments->get_Message(&message);
            arguments->get_DefaultText(&defaultText);

            openDialog = arguments;
            arguments->GetDeferral(&openDialogDeferral);

            std::lock_guard<std::mutex> guard(report.lock);
            report.dialog = ScriptDialog{DialogKindOf(kind), TakeText(message), TakeText(defaultText)};
            return S_OK;
        }).Get(), &token);

    webView->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>(
        [](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* arguments) {
            arguments->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);
            return S_OK;
        }).Get(), &token);

    ComPtr<ICoreWebView2_4> downloads;
    if (SUCCEEDED(webView.As(&downloads)))
    {
        downloads->add_DownloadStarting(Callback<ICoreWebView2DownloadStartingEventHandler>(
            [](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* arguments) {
                arguments->put_Cancel(TRUE);
                arguments->put_Handled(TRUE);
                return S_OK;
            }).Get(), &token);
    }

    ComPtr<ICoreWebView2_5> certificates;
    if (SUCCEEDED(webView.As(&certificates)))
    {
        certificates->add_ClientCertificateRequested(Callback<ICoreWebView2ClientCertificateRequestedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2ClientCertificateRequestedEventArgs* arguments) {
                arguments->put_Handled(TRUE);
                return S_OK;
            }).Get(), &token);
    }

    ComPtr<ICoreWebView2_10> logins;
    if (SUCCEEDED(webView.As(&logins)))
    {
        logins->add_BasicAuthenticationRequested(Callback<ICoreWebView2BasicAuthenticationRequestedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2BasicAuthenticationRequestedEventArgs* arguments) {
                arguments->put_Cancel(TRUE);
                return S_OK;
            }).Get(), &token);
    }
}

void PageEngine::ReportStartError(const std::string& message)
{
    std::lock_guard<std::mutex> guard(report.lock);
    report.startError = message;
}

void PageEngine::ReportLoadStep(bool isLoading, double progress)
{
    std::lock_guard<std::mutex> guard(report.lock);
    report.state.isLoading = isLoading;
    report.state.loadingProgress = progress;
}

void PageEngine::ReportFailure(const std::string& address, LoadError error)
{
    std::lock_guard<std::mutex> guard(report.lock);
    report.failures.push_back({address, error});
}

void PageEngine::ReportPicture(Formats::RgbaImage&& picture)
{
    std::lock_guard<std::mutex> guard(report.lock);
    report.newPicture = std::move(picture);
}

// A page of a Nintendo service goes to the server, or fails when there is none. Returns true when the load was replaced.
bool PageEngine::RedirectIfNeeded(ICoreWebView2NavigationStartingEventArgs& arguments)
{
    LPWSTR wideAddress = nullptr;
    arguments.get_Uri(&wideAddress);
    std::string address = TakeWideText(wideAddress);
    lastStartedAddress = address;

    RedirectDecision decision = DecideRedirect(address, redirect);
    if (!decision.isRedirected)
        return false;

    arguments.put_Cancel(TRUE);
    if (decision.newAddress.empty())
        ReportFailure(address, LoadError::ServerNotSet);
    else
        Open(decision.newAddress);
    return true;
}

void PageEngine::ServeRequest(ICoreWebView2WebResourceRequestedEventArgs& arguments)
{
    ComPtr<ICoreWebView2WebResourceRequest> request;
    arguments.get_Request(&request);
    LPWSTR wideAddress = nullptr;
    request->get_Uri(&wideAddress);
    std::string address = TakeWideText(wideAddress);

    auto host = servedHosts.find(HostOf(address));
    if (host == servedHosts.end())
        return;

    std::string path(PathOf(address));
    path = path.substr(0, path.find_first_of("?#"));
    if (path.starts_with('/'))
        path.erase(0, 1);

    ComPtr<ICoreWebView2WebResourceResponse> response;
    auto file = host->second.find(path);
    if (file == host->second.end())
    {
        environment->CreateWebResourceResponse(nullptr, 404, L"Not Found", L"", &response);
        arguments.put_Response(response.Get());
        return;
    }

    const std::vector<std::uint8_t>& bytes = file->second;
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(bytes.data(), static_cast<UINT>(bytes.size())));
    environment->CreateWebResourceResponse(stream.Get(), 200, L"OK", ContentTypeOf(path).c_str(), &response);
    arguments.put_Response(response.Get());
}

void PageEngine::CallDevTools(const wchar_t* method, const std::string& parameters)
{
    webView->CallDevToolsProtocolMethod(method, WideTextOf(parameters).c_str(), nullptr);
}

void PageEngine::SetViewSize(const ViewSize& size)
{
    viewSize = size;
    PlaceHostWindow();
    if (!controller)
        return;

    ComPtr<ICoreWebView2Controller3> scaleControl;
    if (SUCCEEDED(controller.As(&scaleControl)))
        scaleControl->put_RasterizationScale(viewSize.pixelsPerPagePixel);

    RECT clientArea = {};
    GetClientRect(hostWindow, &clientArea);
    controller->put_Bounds(clientArea);

    float width = static_cast<float>(clientArea.right - clientArea.left);
    float height = static_cast<float>(clientArea.bottom - clientArea.top);
    pageVisual.Size({width, height});
    StartCapture();
}

// The capture of a visual keeps the size the visual had when it started, so a new size starts a new capture.
void PageEngine::StartCapture()
{
    capture.reset();
    try
    {
        capture = std::make_unique<PageCapture>(pageVisual, [this](Formats::RgbaImage&& picture) { ReportPicture(std::move(picture)); });
    }
    catch (const BrowserError& error)
    {
        ReportStartError(error.what());
    }
}

void PageEngine::SetRedirect(const AddressRedirect& newRedirect)
{
    redirect = newRedirect;
}

void PageEngine::ServeFiles(const std::string& hostName, std::map<std::string, std::vector<std::uint8_t>> files)
{
    servedHosts[hostName] = std::move(files);
    std::wstring filter = WideTextOf(std::format("https://{}/*", hostName));
    webView->AddWebResourceRequestedFilter(filter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
}

void PageEngine::Open(const std::string& address)
{
    webView->Navigate(WideTextOf(address).c_str());
}

void PageEngine::GoBack()
{
    webView->GoBack();
}

void PageEngine::GoForward()
{
    webView->GoForward();
}

void PageEngine::Reload()
{
    webView->Reload();
}

void PageEngine::Stop()
{
    webView->Stop();
}

void PageEngine::SetZoom(double zoom)
{
    controller->put_ZoomFactor(zoom);
}

void PageEngine::MovePointer(double pageX, double pageY)
{
    pointerX = pageX;
    pointerY = pageY;
    CallDevTools(L"Input.dispatchMouseEvent", MouseEventParameters(MouseEventKind::Moved, pointerX, pointerY));
}

void PageEngine::PressPointer()
{
    CallDevTools(L"Input.dispatchMouseEvent", MouseEventParameters(MouseEventKind::Pressed, pointerX, pointerY));
}

void PageEngine::ReleasePointer()
{
    CallDevTools(L"Input.dispatchMouseEvent", MouseEventParameters(MouseEventKind::Released, pointerX, pointerY));
}

void PageEngine::Scroll(double pixelsRight, double pixelsDown)
{
    CallDevTools(L"Input.dispatchMouseEvent", WheelEventParameters(pointerX, pointerY, pixelsRight, pixelsDown));
}

void PageEngine::TypeText(const std::u16string& text)
{
    CallDevTools(L"Input.insertText", InsertTextParameters(text));
}

void PageEngine::PressKey(PageKey key)
{
    CallDevTools(L"Input.dispatchKeyEvent", KeyEventParameters(key, true));
    CallDevTools(L"Input.dispatchKeyEvent", KeyEventParameters(key, false));
}

void PageEngine::SetSingleColumn(bool isSingleColumnOn)
{
    isSingleColumn = isSingleColumnOn;
    webView->ExecuteScript(WideTextOf(SingleColumnScript(isSingleColumn)).c_str(), nullptr);
}

void PageEngine::FillTextField(const std::u16string& text)
{
    webView->ExecuteScript(WideTextOf(FillTextFieldScript(text)).c_str(), nullptr);
}

void PageEngine::AnswerScriptDialog(bool isAccepted, const std::u16string& text)
{
    if (!openDialog)
        return;

    if (isAccepted)
    {
        openDialog->put_ResultText(WideTextOf(text).c_str());
        openDialog->Accept();
    }
    openDialogDeferral->Complete();
    openDialog.Reset();
    openDialogDeferral.Reset();
}

void PageEngine::DeleteCookies()
{
    ComPtr<ICoreWebView2_2> cookieOwner;
    if (FAILED(webView.As(&cookieOwner)))
        return;

    ComPtr<ICoreWebView2CookieManager> cookies;
    cookieOwner->get_CookieManager(&cookies);
    cookies->DeleteAllCookies();
}

}
