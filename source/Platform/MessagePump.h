#pragma once

namespace Platform
{

// Handles every message that Windows has queued for the windows of this thread, then returns at once.
// The program calls this once for every picture, so that window events reach their listeners.
void ProcessPendingMessages();

// Sleeps until Windows queues a new message for a window of this thread. The program calls this when there is nothing to draw,
// for example while its window is minimized, so that it does not use the processor in a loop.
// The messages that are queued already must be handled with ProcessPendingMessages first. Only a new message ends the wait.
void WaitForMessage();

}
