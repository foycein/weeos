#include "Browser/Addresses.h"

#include <cctype>

namespace Browser
{

namespace
{

constexpr std::string_view SchemeEnd = "://";

std::string_view AfterScheme(std::string_view address)
{
    std::size_t schemeEnd = address.find(SchemeEnd);
    if (schemeEnd == std::string_view::npos)
        return {};

    return address.substr(schemeEnd + SchemeEnd.size());
}

std::string InSmallLetters(std::string_view text)
{
    std::string small;
    for (char character : text)
        small += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

    return small;
}

}

std::string HostOf(std::string_view address)
{
    std::string_view rest = AfterScheme(address);
    std::size_t hostEnd = rest.find_first_of(":/?#");
    return InSmallLetters(rest.substr(0, hostEnd));
}

std::string_view PathOf(std::string_view address)
{
    std::string_view rest = AfterScheme(address);
    std::size_t pathStart = rest.find_first_of("/?#");
    if (pathStart == std::string_view::npos)
        return {};

    return rest.substr(pathStart);
}

bool BelongsTo(std::string_view host, std::string_view name)
{
    if (host == name)
        return true;

    bool isLonger = host.size() > name.size();
    return isLonger && host.ends_with(name) && host[host.size() - name.size() - 1] == '.';
}

RedirectDecision DecideRedirect(std::string_view address, const AddressRedirect& redirect)
{
    std::string host = HostOf(address);
    for (const std::string& name : redirect.hostNames)
    {
        if (!BelongsTo(host, name))
            continue;

        if (redirect.serverAddress.empty())
            return {true, {}};

        return {true, redirect.serverAddress + std::string(PathOf(address))};
    }
    return {};
}

}
