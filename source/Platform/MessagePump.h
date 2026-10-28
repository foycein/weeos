#pragma once

namespace Platform
{

// Handles every message that Windows has queued for the windows of this thread, then returns at once.
// The program calls this once for every picture, so that window events reach their listeners.
void ProcessPendingMessages();

}
