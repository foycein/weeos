#include "Channels/WiiSpeak/WiiSpeakFriends.h"

#include <array>
#include <string>

namespace Channels
{

namespace
{

constexpr std::size_t DigitsPerGroup = 4;
constexpr std::uint64_t DigitBase = 10;

// A Wii number holds 53 bits. Its scramble is undone in four steps: the mask 0x5E5E..., a turn right by one bit, the bytes put back
// in their places with each half byte replaced (the table of 0x803B9560), a turn left by ten bits, and the mask 0xB3B3....
constexpr int WiiNumberBits = 53;
constexpr std::uint64_t WiiNumberMask = (std::uint64_t{1} << WiiNumberBits) - 1;
constexpr std::uint64_t OuterMask = 0x5E5E5E5E5E5E;
constexpr std::uint64_t InnerMask = 0xB3B3B3B3B3B3;
constexpr int InnerTurnBits = 10;
constexpr int BitsPerByte = 8;
constexpr int BitsPerHalfByte = 4;
constexpr std::uint64_t ByteMask = 0xFF;
constexpr std::uint64_t HalfByteMask = 0xF;
constexpr std::array<std::uint8_t, 16> HalfByteOfHalfByte = {0xD, 0x5, 0x9, 0x7, 0x0, 0xF, 0xA, 0x2, 0xC, 0x3, 0xE, 0x1, 0x8, 0x6, 0xB, 0x4};

// Byte n of the unscrambled number comes from this byte of the turned number. The top five bits (byte 6) stay where they are.
constexpr std::array<int, 6> SourceByteOfByte = {1, 5, 0, 4, 2, 3};
constexpr int UnmovedByte = 6;

// The check bits: the polynomial of ten bits, divided into the 43 bits above them.
constexpr std::uint64_t CheckPolynomial = 0x635;
constexpr int DividedBitCount = 43;

std::uint64_t TurnRight(std::uint64_t bits, int count)
{
    return ((bits >> count) | (bits << (WiiNumberBits - count))) & WiiNumberMask;
}

std::uint64_t ByteOf(std::uint64_t bits, int byteNumber)
{
    return (bits >> (byteNumber * BitsPerByte)) & ByteMask;
}

std::uint64_t ReplaceHalfBytes(std::uint64_t byte)
{
    std::uint64_t high = HalfByteOfHalfByte[(byte >> BitsPerHalfByte) & HalfByteMask];
    std::uint64_t low = HalfByteOfHalfByte[byte & HalfByteMask];
    return (high << BitsPerHalfByte) | low;
}

std::uint64_t UnscrambleWiiNumber(std::uint64_t wiiNumber)
{
    std::uint64_t turned = TurnRight((wiiNumber ^ OuterMask) & WiiNumberMask, 1);

    std::uint64_t placed = ByteOf(turned, UnmovedByte) << (UnmovedByte * BitsPerByte);
    for (int byteNumber = 0; byteNumber < static_cast<int>(SourceByteOfByte.size()); ++byteNumber)
        placed |= ReplaceHalfBytes(ByteOf(turned, SourceByteOfByte[byteNumber])) << (byteNumber * BitsPerByte);

    return TurnRight(placed, WiiNumberBits - InnerTurnBits) ^ InnerMask;
}

}

bool IsValidWiiNumber(std::uint64_t wiiNumber)
{
    std::uint64_t remainder = UnscrambleWiiNumber(wiiNumber);
    for (int step = 0; step < DividedBitCount; ++step)
    {
        int bit = WiiNumberBits - 1 - step;
        if ((remainder >> bit) & 1)
            remainder ^= CheckPolynomial << (DividedBitCount - 1 - step);
    }
    return remainder == 0;
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
