#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Channels
{

// A Wii Friend of the address book: the 16 digits of the friend's Wii number and the nickname the user gave the friend.
struct WiiFriend
{
    std::uint64_t wiiNumber = 0;
    std::u16string nickname;
};

enum class FriendServerAnswer
{
    Done,

    // No server answered; the channel shows the WiiConnect24 window that the Internet cannot be reached.
    Unreachable,
};

// What the Wii Speak Channel needs from a server for its friends: the Wii number of this console and the Wii Friends of its address
// book. On a Wii these come from WiiConnect24, which WeeOS does not have (docs/DECISIONS.md); the owner's server will answer through
// this interface later. Until then OfflineFriendServer stands in.
class WiiSpeakFriendServer
{
public:
    virtual ~WiiSpeakFriendServer() = default;

    // The Wii number the server gave this console. Nothing while there is none.
    virtual std::optional<std::uint64_t> OwnWiiNumber() = 0;

    virtual std::vector<WiiFriend> Friends() = 0;
    virtual FriendServerAnswer AddFriend(const WiiFriend& wiiFriend) = 0;
};

// No server: no Wii number, no Wii Friends, and nothing can be added.
class OfflineFriendServer : public WiiSpeakFriendServer
{
public:
    std::optional<std::uint64_t> OwnWiiNumber() override;
    std::vector<WiiFriend> Friends() override;
    FriendServerAnswer AddFriend(const WiiFriend& wiiFriend) override;
};

constexpr std::size_t WiiNumberDigits = 16;

// "1234 5678 9012 3456", as the Wii shows a Wii number.
std::u16string WiiNumberText(std::uint64_t wiiNumber);

// The number of 16 digits that the user typed. Nothing when the text is not 16 digits.
std::optional<std::uint64_t> ReadWiiNumber(std::u16string_view typed);

}
