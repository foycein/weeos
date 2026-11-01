#pragma once

#include "Channels/Internet/OperaData.h"

#include <string>
#include <string_view>
#include <vector>

namespace Channels
{

// The address of what was typed on the keyboard of "Web Address": spaces at the ends go, and an address without a scheme gets http://
// ("You can omit the http:// portion of Website addresses", the manual page 6). Empty when nothing was typed.
std::string AddressOfTypedText(std::u16string_view typedText);

// The address of a search for the words with the chosen engine; the words are written in UTF-8 with the signs of an address escaped.
std::string SearchAddressOf(SearchEngine engine, std::u16string_view words);

// The hosts of Nintendo's services for the Wii: their pages go to the owner's server, and fail while there is none (docs/DECISIONS.md,
// Online). Assumed: the services the channel's own pages and favorites lead to.
std::vector<std::string> WiiServiceHosts();

}
