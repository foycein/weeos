#pragma once

#include "SystemMenu/WiiSettings/PageContent.h"
#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageId.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Settings/ConsoleClock.h"
#include "Settings/SystemSettings.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// What the pages of Wii Settings work with. The services must outlive the flow.
struct SettingsFlowServices
{
    Settings::SystemSettings& settings;
    const Settings::ConsoleClock& clock;
};

// The pages of Wii Settings and the way from one to the next. It knows nothing about pictures: it says which page is shown and what is on it,
// and it does what a button does when the screen tells it that the button was pressed.
class SettingsFlow : public PageNavigator
{
public:
    // Starts on the page. The services must outlive the flow.
    SettingsFlow(const SettingsFlowServices& services, PageId firstPage);
    ~SettingsFlow() override;

    SettingsFlow(const SettingsFlow&) = delete;
    SettingsFlow& operator=(const SettingsFlow&) = delete;

    PageId CurrentPageId() const;
    const Page& CurrentPage() const;

    // Does what the button does. A button that is not shown or not enabled does nothing.
    void PressButton(std::size_t buttonIndex);

    // The same for the button with this name. Gives back false when the page has no such button. For the tests.
    bool PressButtonNamed(std::string_view name);

    // One step of time passes.
    void Step();

    // True when the user left Wii Settings.
    bool IsFinished() const;

    // What the user asked for with the last button and has not been given yet.
    const std::optional<InputRequest>& PendingInput() const;
    void AnswerInput(const std::u16string& answer);
    void CancelInput();

    // The sounds that the buttons asked for since the last call.
    std::vector<PageSound> TakeSounds();

    void GoTo(PageId page) override;
    void Refresh() override;
    void LeaveSettings() override;
    void RequestInput(const InputRequest& request, std::function<void(std::optional<std::u16string>)> onAnswer) override;

private:
    PageGroup& GroupOf(PageId page);
    void MakeGroups(const SettingsFlowServices& services);

    std::vector<std::unique_ptr<PageGroup>> groups;
    PageId currentPageId;
    Page currentPage;
    bool isFinished = false;

    std::optional<InputRequest> pendingInput;
    std::function<void(std::optional<std::u16string>)> inputAnswerHandler;
    std::vector<PageSound> pendingSounds;
};

}
