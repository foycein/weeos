#include "Ios/Power/PowerService.h"

namespace Ios
{

void PowerService::ReportButtonPress(PowerEvent event)
{
    events.Post(event);
}

std::optional<PowerEvent> PowerService::WaitForEvent(std::stop_token stopToken)
{
    return events.Wait(stopToken);
}

std::optional<PowerEvent> PowerService::TryTakeEvent()
{
    return events.TryTake();
}

void PowerService::RequestReset()
{
    requests.Post(PowerRequest::Reset);
}

void PowerService::RequestShutdown()
{
    requests.Post(PowerRequest::Shutdown);
}

EventQueue<PowerRequest>& PowerService::Requests()
{
    return requests;
}

void PowerService::SetSlotLightMode(SlotLightMode mode)
{
    slotLightMode = mode;
}

SlotLightMode PowerService::GetSlotLightMode() const
{
    return slotLightMode;
}

void PowerService::Close()
{
    events.Close();
    requests.Close();
}

}
