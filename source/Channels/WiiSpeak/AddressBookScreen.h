#pragma once

#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakFriends.h"
#include "Channels/WiiSpeak/WiiSpeakSession.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Channels
{

// The address book (scene friendList, 06000_Flist): the Wii Friends of the friend server five to a page with the arrows, Add Wii
// Friend, OK and Help. Add Wii Friend asks on the keyboard for the 16 digits of the friend's Wii number and then a nickname, and
// gives the friend to the server. The number must be 16 digits, not this console's own and not one the book has; a full book (100
// friends) takes no more.
class AddressBookScreen : public WiiSpeakScreen
{
public:
    explicit AddressBookScreen(WiiSpeakSession& session);

    std::optional<WiiSpeakScreenName> Update() override;
    void Draw() override;
    std::vector<SceneLayout*> ShownLayouts() override;

    std::size_t ShownFriendCount() const;

private:
    enum class Typing
    {
        Nothing,
        WiiNumber,
        Nickname,
    };

    std::optional<WiiSpeakScreenName> Press(const std::string& button);
    void TurnPage(bool isForward);
    bool IsKnownFriend(std::uint64_t wiiNumber) const;
    void StartAddingAFriend();
    void TakeTheWiiNumber(const std::u16string& typed);
    void TakeTheNickname(const std::u16string& typed);
    void OpenKeyboard(std::uint32_t titleMessageId, std::size_t maximumLength, SystemMenu::CharacterRule allowedCharacters);
    void ShowPage();
    std::size_t PageCount() const;

    WiiSpeakSession& session;
    SceneLayout layout;
    std::vector<WiiFriend> friends;
    std::size_t page = 0;
    Typing typing = Typing::Nothing;
    std::uint64_t typedWiiNumber = 0;
};

}
