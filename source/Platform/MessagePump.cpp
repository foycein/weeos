#include "Platform/MessagePump.h"

#include <windows.h>

namespace Platform
{

void ProcessPendingMessages()
{
    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void WaitForMessage()
{
    WaitMessage();
}

}
