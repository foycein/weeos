#include "SystemMenu/WiiSettings/SettingsFlow.h"

#include "SystemMenu/MenuError.h"
#include "SystemMenu/WiiSettings/ConnectionPages.h"
#include "SystemMenu/WiiSettings/ImportPages.h"
#include "SystemMenu/WiiSettings/IndexPages.h"
#include "SystemMenu/WiiSettings/NicknamePage.h"
#include "SystemMenu/WiiSettings/PcPages.h"
#include "SystemMenu/WiiSettings/SensorBarPages.h"
#include "SystemMenu/WiiSettings/SoundPage.h"

#include <format>

namespace SystemMenu
{

SettingsFlow::SettingsFlow(const SettingsFlowServices& services, PageId firstPage)
    : currentPageId(firstPage)
{
    MakeGroups(services);
    GoTo(firstPage);
}

SettingsFlow::~SettingsFlow() = default;

void SettingsFlow::MakeGroups(const SettingsFlowServices& services)
{
    groups.push_back(std::make_unique<IndexPages>(*this));
    groups.push_back(std::make_unique<NicknamePage>(*this, services.settings));
    groups.push_back(std::make_unique<SensorBarPages>(*this, services.settings));
    groups.push_back(std::make_unique<ConnectionPages>(*this, services.networkConfig, services.sockets, services.pcSettings));
    bool canImport = services.consoleImport != nullptr;
    groups.push_back(std::make_unique<PcPages>(*this, services.settings, services.pcSettings, services.pcTarget, canImport));
    groups.push_back(std::make_unique<SoundPage>(*this, services.settings));
    if (canImport)
        groups.push_back(std::make_unique<ImportPages>(*this, *services.consoleImport));
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
    GiveAnswer(answer);
}

void SettingsFlow::CancelInput()
{
    GiveAnswer(std::nullopt);
}

// The request is taken away first, because the answer may ask for something again.
void SettingsFlow::GiveAnswer(std::optional<std::u16string> answer)
{
    if (!pendingInput)
        return;

    std::function<void(std::optional<std::u16string>)> handler = std::move(inputAnswerHandler);
    pendingInput.reset();
    inputAnswerHandler = nullptr;
    handler(answer);
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
