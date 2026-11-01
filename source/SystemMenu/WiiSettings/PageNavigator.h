#pragma once

#include "SystemMenu/WiiSettings/InputRequest.h"
#include "SystemMenu/WiiSettings/PageContent.h"
#include "SystemMenu/WiiSettings/PageId.h"

#include <functional>
#include <optional>
#include <string>

namespace SystemMenu
{

// What a group of pages can ask of the flow that shows them.
class PageNavigator
{
public:
    virtual ~PageNavigator() = default;

    virtual void GoTo(PageId page) = 0;

    // The page that is shown is built again, because what it shows has changed.
    virtual void Refresh() = 0;

    // Wii Settings ends and the menu goes back to where the user came from.
    virtual void LeaveSettings() = 0;

    // Asks the user for something. The answer is nothing when the user cancelled.
    virtual void RequestInput(const InputRequest& request, std::function<void(std::optional<std::u16string>)> onAnswer) = 0;
};

}
