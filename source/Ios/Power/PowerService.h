#pragma once

#include "Ios/EventQueue.h"

#include <cstdint>
#include <optional>
#include <stop_token>

namespace Ios
{

// What the front panel of the console reports to the programs.
enum class PowerEvent
{
    ResetButtonPressed,
    PowerButtonPressed,
};

// The word that the original event hook (/dev/stm/eventhook) hands to the program for an event. The native code does not need it.
// It is here so that a log line, or a result measured on the console, can be compared with ours.
constexpr std::uint32_t ResetButtonEventCode = 0x00020000;
constexpr std::uint32_t PowerButtonEventCode = 0x00000800;

constexpr std::uint32_t OriginalEventCode(PowerEvent event)
{
    if (event == PowerEvent::ResetButtonPressed)
        return ResetButtonEventCode;

    return PowerButtonEventCode;
}

// What a program asks of the console. The platform carries it out.
enum class PowerRequest
{
    // Restart the whole console session (HotReset of the original).
    Reset,

    // Switch the console off (Shutdown of the original).
    Shutdown,
};

// The power and reset buttons and the requests to reset or shut down, in place of /dev/stm/eventhook and /dev/stm/immediate.
//
// The platform reports the buttons (the keys of the window stand for them). The menu and the channels wait for them, and ask the platform
// to reset or shut down. All of it is thread safe.
//
// The original loses a button press when no program waits for it. Here a press is kept until a program takes it (deviation D1 in docs/audit/Ios.md).
class PowerService
{
public:
    // The platform tells that a button was pressed. Events are kept in the order they come.
    void ReportButtonPress(PowerEvent event);

    // Waits until a button was pressed and gives it back. Each event goes to one waiting program only.
    // Gives back nothing when the stop token asks to stop, or when the service was closed and no event is left.
    std::optional<PowerEvent> WaitForEvent(std::stop_token stopToken);

    // Does not wait. Gives back nothing when no button was pressed.
    std::optional<PowerEvent> TryTakeEvent();

    // A program asks the platform to restart the console, or to switch it off.
    void RequestReset();
    void RequestShutdown();

    // The requests that the programs made, in order. The platform waits here and carries them out.
    EventQueue<PowerRequest>& Requests();

    // Ends the service: whoever waits for an event or a request stops waiting, and nothing new is accepted.
    void Close();

private:
    EventQueue<PowerEvent> events;
    EventQueue<PowerRequest> requests;
};

}
