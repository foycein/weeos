#pragma once

#include "Ios/Network/Ipv4Address.h"

#include <functional>

namespace Ios
{

// Tells whether the PC is connected to a network. WeeOS is online through the PC's connection, with nothing to set up
// (docs/DECISIONS.md, Online): when the PC is connected, the console's link is up.
using PcConnectionCheck = std::function<bool()>;

// Asks Windows. The PC counts as connected when Windows reaches the Internet or at least the local network, where the owner's
// own server will live (docs/audit/OpenPoints/Ios.md NCD3).
bool IsWindowsConnected();

// The IPv4 address of the PC in its network: the first address of a network adapter that is up, is not the loopback adapter and
// has a gateway. 0.0.0.0 when there is none.
Ipv4Address FindPcAddress();

}
