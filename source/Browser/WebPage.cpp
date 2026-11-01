#include "Browser/WebPage.h"

#include "Browser/PageEngine.h"
#include "Browser/PageThread.h"

namespace Browser
{

WebPage::WebPage(const BrowserSettings& settings)
    : thread(std::make_unique<PageThread>(settings))
{
}

WebPage::~WebPage() = default;

bool WebPage::IsReady() const
{
    PageReport& report = thread->Report();
    std::lock_guard<std::mutex> guard(report.lock);
    return report.isReady;
}

std::optional<std::string> WebPage::StartError() const
{
    PageReport& report = thread->Report();
    std::lock_guard<std::mutex> guard(report.lock);
    return report.startError;
}

void WebPage::SetViewSize(const ViewSize& size)
{
    thread->Send([size](PageEngine& engine) { engine.SetViewSize(size); });
}

void WebPage::SetRedirect(const AddressRedirect& redirect)
{
    thread->Send([redirect](PageEngine& engine) { engine.SetRedirect(redirect); });
}

void WebPage::ServeFiles(const std::string& hostName, std::map<std::string, std::vector<std::uint8_t>> files)
{
    auto sharedFiles = std::make_shared<std::map<std::string, std::vector<std::uint8_t>>>(std::move(files));
    thread->Send([hostName, sharedFiles](PageEngine& engine) { engine.ServeFiles(hostName, std::move(*sharedFiles)); });
}

void WebPage::Open(const std::string& address)
{
    thread->Send([address](PageEngine& engine) { engine.Open(address); });
}

void WebPage::GoBack()
{
    thread->Send([](PageEngine& engine) { engine.GoBack(); });
}

void WebPage::GoForward()
{
    thread->Send([](PageEngine& engine) { engine.GoForward(); });
}

void WebPage::Reload()
{
    thread->Send([](PageEngine& engine) { engine.Reload(); });
}

void WebPage::Stop()
{
    thread->Send([](PageEngine& engine) { engine.Stop(); });
}

void WebPage::SetZoom(double newZoom)
{
    zoom = newZoom;
    thread->Send([newZoom](PageEngine& engine) { engine.SetZoom(newZoom); });
}

double WebPage::Zoom() const
{
    return zoom;
}

void WebPage::MovePointer(double pageX, double pageY)
{
    thread->Send([pageX, pageY](PageEngine& engine) { engine.MovePointer(pageX, pageY); });
}

void WebPage::PressPointer()
{
    thread->Send([](PageEngine& engine) { engine.PressPointer(); });
}

void WebPage::ReleasePointer()
{
    thread->Send([](PageEngine& engine) { engine.ReleasePointer(); });
}

void WebPage::Scroll(double pixelsRight, double pixelsDown)
{
    thread->Send([pixelsRight, pixelsDown](PageEngine& engine) { engine.Scroll(pixelsRight, pixelsDown); });
}

void WebPage::TypeText(const std::u16string& text)
{
    thread->Send([text](PageEngine& engine) { engine.TypeText(text); });
}

void WebPage::PressKey(PageKey key)
{
    thread->Send([key](PageEngine& engine) { engine.PressKey(key); });
}

void WebPage::FillTextField(const std::u16string& text)
{
    thread->Send([text](PageEngine& engine) { engine.FillTextField(text); });
}

void WebPage::AnswerScriptDialog(bool isAccepted, const std::u16string& text)
{
    thread->Send([isAccepted, text](PageEngine& engine) { engine.AnswerScriptDialog(isAccepted, text); });
}

void WebPage::DeleteCookies()
{
    thread->Send([](PageEngine& engine) { engine.DeleteCookies(); });
}

PageState WebPage::State() const
{
    PageReport& report = thread->Report();
    std::lock_guard<std::mutex> guard(report.lock);
    return report.state;
}

std::optional<Formats::RgbaImage> WebPage::TakeNewPicture()
{
    PageReport& report = thread->Report();
    std::lock_guard<std::mutex> guard(report.lock);
    std::optional<Formats::RgbaImage> picture = std::move(report.newPicture);
    report.newPicture.reset();
    return picture;
}

std::optional<LoadFailure> WebPage::TakeLoadFailure()
{
    PageReport& report = thread->Report();
    std::lock_guard<std::mutex> guard(report.lock);
    if (report.failures.empty())
        return std::nullopt;

    LoadFailure failure = report.failures.front();
    report.failures.pop_front();
    return failure;
}

std::optional<ScriptDialog> WebPage::TakeScriptDialog()
{
    PageReport& report = thread->Report();
    std::lock_guard<std::mutex> guard(report.lock);
    std::optional<ScriptDialog> dialog = std::move(report.dialog);
    report.dialog.reset();
    return dialog;
}

std::optional<TextFieldRequest> WebPage::TakeTextFieldRequest()
{
    PageReport& report = thread->Report();
    std::lock_guard<std::mutex> guard(report.lock);
    std::optional<TextFieldRequest> request = std::move(report.textField);
    report.textField.reset();
    return request;
}

}
