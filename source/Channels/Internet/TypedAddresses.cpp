#include "Channels/Internet/TypedAddresses.h"

#include "Browser/TextConversion.h"

#include <cctype>
#include <format>

namespace Channels
{

namespace
{

constexpr std::string_view SchemeMark = "://";
constexpr std::string_view PlainWebScheme = "http://";

// The search address of the original's Google (operarc defaults: Search Engine URL 0) without its Opera parameters, over https; Yahoo's
// address of the program was not found, so it is Yahoo's own search page (Assumed).
constexpr std::string_view GoogleSearchAddress = "https://www.google.com/search?q={}&ie=utf-8&oe=utf-8";
constexpr std::string_view YahooSearchAddress = "https://search.yahoo.com/search?p={}";

bool IsUnreservedCharacter(unsigned char character)
{
    return std::isalnum(character) || character == '-' || character == '_' || character == '.' || character == '~';
}

// Spaces become plus signs, as a form sends them; every other sign that means something in an address is written as %XX.
std::string EscapedWords(std::u16string_view words)
{
    std::string escaped;
    for (unsigned char character : Browser::Utf8TextOf(words))
    {
        if (character == ' ')
            escaped += '+';
        else if (IsUnreservedCharacter(character))
            escaped += static_cast<char>(character);
        else
            escaped += std::format("%{:02X}", character);
    }
    return escaped;
}

}

std::string AddressOfTypedText(std::u16string_view typedText)
{
    std::string address = Browser::Utf8TextOf(typedText);
    while (!address.empty() && address.front() == ' ')
        address.erase(address.begin());
    while (!address.empty() && address.back() == ' ')
        address.pop_back();

    if (address.empty() || address.find(SchemeMark) != std::string::npos)
        return address;

    return std::string(PlainWebScheme) + address;
}

std::string SearchAddressOf(SearchEngine engine, std::u16string_view words)
{
    std::string_view pattern = engine == SearchEngine::Yahoo ? YahooSearchAddress : GoogleSearchAddress;
    std::string escapedWords = EscapedWords(words);
    return std::vformat(pattern, std::make_format_args(escapedWords));
}

std::vector<std::string> WiiServiceHosts()
{
    return {"rs.nintendo.com", "nintendowifi.net", "nintendo.net", "wii.com"};
}

}
