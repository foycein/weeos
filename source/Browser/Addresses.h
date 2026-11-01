#pragma once

#include "Browser/WebPage.h"

#include <optional>
#include <string>
#include <string_view>

namespace Browser
{

// The host of an address in small letters: "www.example.com" of "https://WWW.Example.com:8080/page?x=1". Empty when there is none.
std::string HostOf(std::string_view address);

// Everything after the host and the port: "/page?x=1". Empty when there is nothing.
std::string_view PathOf(std::string_view address);

// True when the host is the name or one of its subdomains: "rs.nintendo.com" belongs to "nintendo.com", "notnintendo.com" does not.
bool BelongsTo(std::string_view host, std::string_view name);

// What happens to a page whose host the redirect names.
struct RedirectDecision
{
    bool isRedirected = false;

    // The address on the server, or empty when there is no server and the page cannot be shown.
    std::string newAddress;
};

RedirectDecision DecideRedirect(std::string_view address, const AddressRedirect& redirect);

}
