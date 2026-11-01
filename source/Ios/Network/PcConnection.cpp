#include "Ios/Network/PcConnection.h"

#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include <vector>

namespace Ios
{

bool IsWindowsConnected()
{
    NL_NETWORK_CONNECTIVITY_HINT hint = {};
    if (GetNetworkConnectivityHint(&hint) != NO_ERROR)
        return false;

    switch (hint.ConnectivityLevel)
    {
    case NetworkConnectivityLevelHintLocalAccess:
    case NetworkConnectivityLevelHintInternetAccess:
    case NetworkConnectivityLevelHintConstrainedInternetAccess:
        return true;
    default:
        return false;
    }
}

namespace
{

constexpr Ipv4Address NoAddress = 0;

// Windows says how many bytes the list needs when the first try is too small. Three tries are enough even when an adapter comes
// between them.
constexpr int TriesToReadTheAdapters = 3;
constexpr ULONG FirstGuessOfTheListSize = 16 * 1024;

std::vector<unsigned char> ReadAdapterList()
{
    ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG size = FirstGuessOfTheListSize;
    for (int attempt = 0; attempt < TriesToReadTheAdapters; ++attempt)
    {
        std::vector<unsigned char> list(size);
        auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(list.data());
        ULONG result = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size);
        if (result == NO_ERROR)
            return list;

        if (result != ERROR_BUFFER_OVERFLOW)
            return {};
    }
    return {};
}

bool IsAdapterOfTheNetwork(const IP_ADAPTER_ADDRESSES& adapter)
{
    if (adapter.OperStatus != IfOperStatusUp)
        return false;

    if (adapter.IfType == IF_TYPE_SOFTWARE_LOOPBACK)
        return false;

    return adapter.FirstGatewayAddress != nullptr && adapter.FirstUnicastAddress != nullptr;
}

Ipv4Address AddressOf(const SOCKET_ADDRESS& socketAddress)
{
    const auto* address = reinterpret_cast<const sockaddr_in*>(socketAddress.lpSockaddr);
    return ntohl(address->sin_addr.s_addr);
}

}

Ipv4Address FindPcAddress()
{
    std::vector<unsigned char> list = ReadAdapterList();
    if (list.empty())
        return NoAddress;

    for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(list.data()); adapter != nullptr; adapter = adapter->Next)
    {
        if (IsAdapterOfTheNetwork(*adapter))
            return AddressOf(adapter->FirstUnicastAddress->Address);
    }
    return NoAddress;
}

}
