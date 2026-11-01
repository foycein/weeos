#include "SystemMenu/WiiSettings/SettingsFlow.h"

#include "SystemMenu/MenuError.h"
#include "SystemMenu/WiiSettings/CalendarPages.h"
#include "SystemMenu/WiiSettings/ConnectionTestPages.h"
#include "SystemMenu/WiiSettings/CountryPages.h"
#include "SystemMenu/WiiSettings/DisplayPages.h"
#include "SystemMenu/WiiSettings/FormatPages.h"
#include "SystemMenu/WiiSettings/IndexPages.h"
#include "SystemMenu/WiiSettings/InternetPages.h"
#include "SystemMenu/WiiSettings/LanguagePage.h"
#include "SystemMenu/WiiSettings/NicknamePage.h"
#include "SystemMenu/WiiSettings/ParentalPages.h"
#include "SystemMenu/WiiSettings/PcPages.h"
#include "SystemMenu/WiiSettings/SensorBarPages.h"
#include "SystemMenu/WiiSettings/SoundPage.h"
#include "SystemMenu/WiiSettings/UpdatePages.h"

#include <format>

namespace SystemMenu
{

namespace
{

// The radio searches every channel from 1 to 13 for this long on each.
constexpr std::uint16_t EveryChannel = 0x3FFE;
constexpr std::uint16_t MillisecondsPerChannel = 100;

}

AccessPointSearch SearchWithRadio(const Ios::WirelessService& wireless)
{
    return [&wireless]
    {
        Ios::ScanOptions options;
        options.channelMask = EveryChannel;
        options.maximumChannelTimeInMilliseconds = MillisecondsPerChannel;

        Ios::Result<std::vector<Ios::AccessPointInformation>> found = wireless.Scan(options);
        if (found.Failed())
            return std::vector<Ios::AccessPointInformation>();

        return found.Value();
    };
}

SettingsFlow::SettingsFlow(const SettingsFlowServices& services, const Text::MessageCatalog& messages, PageId firstPage)
    : currentPageId(firstPage)
{
    MakeGroups(services, messages);
    GoTo(firstPage);
}

SettingsFlow::~SettingsFlow() = default;

void SettingsFlow::MakeGroups(const SettingsFlowServices& services, const Text::MessageCatalog& messages)
{
    auto connectionTest = std::make_unique<ConnectionTestPages>(*this, services.networkConfig, services.sockets, services.pcSettings, messages);
    ConnectionTestPages& connectionTestPages = *connectionTest;
    groups.push_back(std::move(connectionTest));

    groups.push_back(std::make_unique<IndexPages>(*this, services.settings));
    groups.push_back(std::make_unique<CalendarPages>(*this, services.settings, services.clock));
    groups.push_back(std::make_unique<DisplayPages>(*this, services.settings, services.pcTarget));
    groups.push_back(std::make_unique<SoundPage>(*this, services.settings));
    groups.push_back(std::make_unique<SensorBarPages>(*this, services.settings));
    groups.push_back(std::make_unique<LanguagePage>(*this, services.settings));
    groups.push_back(std::make_unique<NicknamePage>(*this, services.settings));
    groups.push_back(std::make_unique<CountryPages>(*this, services.settings));
    groups.push_back(std::make_unique<FormatPages>(*this, services.formatter));
    groups.push_back(std::make_unique<UpdatePages>(*this, services.networkConfig, connectionTestPages, messages));
    groups.push_back(std::make_unique<InternetPages>(*this, services.networkConfig, connectionTestPages, services.searchForAccessPoints));
    groups.push_back(std::make_unique<ParentalPages>(*this, services.settings, services.clock, services.networkConfig));
    groups.push_back(std::make_unique<PcPages>(*this, services.settings, services.pcSettings, services.pcTarget));
}

PageId SettingsFlow::CurrentPageId() const
{
    return currentPageId;
}

const Page& SettingsFlow::CurrentPage() const
{
    return currentPage;
}

void SettingsFlow::PressButton(std::size_t buttonIndex)
{
    if (buttonIndex >= currentPage.buttons.size())
        return;

    // The button is copied, because its action may build the page again.
    PageButton button = currentPage.buttons[buttonIndex];
    if (!button.isShown || !button.isEnabled)
        return;

    pendingSounds.push_back(button.sound);
    if (button.onPress)
        button.onPress();
}

bool SettingsFlow::PressButtonNamed(std::string_view name)
{
    for (std::size_t buttonIndex = 0; buttonIndex < currentPage.buttons.size(); ++buttonIndex)
    {
        if (currentPage.buttons[buttonIndex].name == name)
        {
            PressButton(buttonIndex);
            return true;
        }
    }

    return false;
}

void SettingsFlow::Step()
{
    GroupOf(currentPageId).Step(currentPageId);
}

bool SettingsFlow::IsFinished() const
{
    return isFinished;
}

const std::optional<InputRequest>& SettingsFlow::PendingInput() const
{
    return pendingInput;
}

void SettingsFlow::AnswerInput(const std::u16string& answer)
{
    if (!pendingInput)
        return;

    // The request is taken away first, because the answer may ask for something again.
    std::function<void(std::optional<std::u16string>)> handler = std::move(inputAnswerHandler);
    pendingInput.reset();
    inputAnswerHandler = nullptr;
    handler(answer);
}

void SettingsFlow::CancelInput()
{
    if (!pendingInput)
        return;

    std::function<void(std::optional<std::u16string>)> handler = std::move(inputAnswerHandler);
    pendingInput.reset();
    inputAnswerHandler = nullptr;
    handler(std::nullopt);
}

std::vector<PageSound> SettingsFlow::TakeSounds()
{
    std::vector<PageSound> sounds = std::move(pendingSounds);
    pendingSounds.clear();
    return sounds;
}

void SettingsFlow::GoTo(PageId page)
{
    currentPageId = page;
    GroupOf(page).EnterPage(page);
    Refresh();
}

void SettingsFlow::Refresh()
{
    currentPage = Page();
    GroupOf(currentPageId).BuildPage(currentPageId, currentPage);
}

void SettingsFlow::LeaveSettings()
{
    isFinished = true;
}

void SettingsFlow::RequestInput(const InputRequest& request, std::function<void(std::optional<std::u16string>)> onAnswer)
{
    pendingInput = request;
    inputAnswerHandler = std::move(onAnswer);
}

PageGroup& SettingsFlow::GroupOf(PageId page)
{
    for (std::unique_ptr<PageGroup>& group : groups)
    {
        if (group->Owns(page))
            return *group;
    }

    throw MenuError(std::format("The page {} of Wii Settings is not built", static_cast<int>(page)));
}

}
