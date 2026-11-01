#include "Channels/WiiSpeak/WiiSpeakFriends.h"

#include <string>

namespace Channels
{

namespace
{

constexpr std::size_t DigitsPerGroup = 4;
constexpr std::uint64_t DigitBase = 10;

}

std::optional<std::uint64_t> OfflineFriendServer::OwnWiiNumber()
{
    return std::nullopt;
}

std::vector<WiiFriend> OfflineFriendServer::Friends()
{
    return {};
}

FriendServerAnswer OfflineFriendServer::AddFriend(const WiiFriend&)
{
    return FriendServerAnswer::Unreachable;
}

std::u16string WiiNumberText(std::uint64_t wiiNumber)
{
    std::u16string digits(WiiNumberDigits, u'0');
    for (std::size_t place = WiiNumberDigits; place > 0; --place)
    {
        digits[place - 1] = static_cast<char16_t>(u'0' + wiiNumber % DigitBase);
        wiiNumber /= DigitBase;
    }

    std::u16string text;
    for (std::size_t place = 0; place < WiiNumberDigits; ++place)
    {
        if (place > 0 && place % DigitsPerGroup == 0)
            text += u' ';
        text += digits[place];
    }
    return text;
}

std::optional<std::uint64_t> ReadWiiNumber(std::u16string_view typed)
{
    if (typed.size() != WiiNumberDigits)
        return std::nullopt;

    std::uint64_t number = 0;
    for (char16_t character : typed)
    {
        if (character < u'0' || character > u'9')
            return std::nullopt;

        number = number * DigitBase + static_cast<std::uint64_t>(character - u'0');
    }
    return number;
}

}
