#include "Channels/WiiSpeak/AddressBookScreen.h"

#include "Channels/ChannelText.h"
#include "Channels/WiiSpeak/WiiSpeakMessageIds.h"

#include <algorithm>
#include <format>
#include <string>

namespace Channels
{

namespace
{

constexpr std::size_t FriendsPerPage = 5;
constexpr std::size_t MostFriends = 100;
constexpr std::size_t DigitBase = 10;

// Assumed (CHN-38): a nickname takes as many characters as the nicknames of the Wii's address book.
constexpr std::size_t NicknameLength = 10;

const std::string AddButton = "nbutton_03";
const std::string OkButton = "dbutton_04";
const std::string HelpButton = "B_dbutton_05";
const std::string LeftButton = "B_nbutton_l";
const std::string RightButton = "B_nbutton_r";

// The sounds of the address book, by their names in MicChatSound.brsar.
constexpr std::string_view AddSound = "SE_ADD_CLK_BTN_NEW";
constexpr std::string_view OkSound = "SE_ADD_CLK_BTN_RET";
constexpr std::string_view HelpSound = "SE_ADD_CLK_BTN_HELP";
constexpr std::string_view NextPageSound = "SE_ADD_CLK_NEXT_GRP";
constexpr std::string_view PreviousPageSound = "SE_ADD_CLK_PREV_GRP";

std::string Numbered(std::string_view name, std::size_t number)
{
    return std::format("{}_{:02}", name, number);
}

std::u16string TensDigit(std::size_t number)
{
    return AsciiToText(std::to_string(number / DigitBase % DigitBase));
}

std::u16string OnesDigit(std::size_t number)
{
    return AsciiToText(std::to_string(number % DigitBase));
}

}

AddressBookScreen::AddressBookScreen(WiiSpeakSession& bookSession)
    : session(bookSession)
    , layout(session.services.display, session.files.SceneLayouts("friendList", "06000_Flist.arc"), "06000_Flist")
    , friends(session.friends.Friends())
{
    for (const std::string& button : {AddButton, OkButton, HelpButton, LeftButton, RightButton})
        layout.AddButton(button);

    layout.SetText("T_Flist", session.Text(WiiSpeakText::AddressBookTitle));
    layout.SetText("T_shadow", session.Text(WiiSpeakText::AddressBookTitle));
    layout.SetText("T_button_03", session.Text(WiiSpeakText::AddWiiFriend));
    layout.SetText("T_button_04", session.Text(WiiSpeakText::AddressBookOk));
    layout.SetText("T_button_05", session.Text(WiiSpeakText::AddressBookHelp));
    layout.SetVisible("N_root_01", false);
    layout.PlayAnimation("all_inOut");
    ShowPage();
}

std::optional<WiiSpeakScreenName> AddressBookScreen::Update()
{
    std::optional<WiiSpeakScreenName> nextScreen;
    std::optional<SystemMenu::KeyboardAnswer> answer = session.keyboard.TakeAnswer();
    if (answer && answer->wasConfirmed && typing == Typing::WiiNumber)
        TakeTheWiiNumber(answer->text);
    else if (answer && answer->wasConfirmed && typing == Typing::Nickname)
        TakeTheNickname(answer->text);
    else if (answer)
        typing = Typing::Nothing;

    if (session.dialog.IsOpen())
        session.dialog.TakeChoice();
    else
    {
        std::optional<std::string> pressed = layout.TakePressedButton(session.services.input);
        if (pressed)
            nextScreen = Press(*pressed);
    }

    layout.Calculate();
    return nextScreen;
}

void AddressBookScreen::Draw()
{
    layout.Draw();
}

std::vector<SceneLayout*> AddressBookScreen::ShownLayouts()
{
    return {&layout};
}

std::size_t AddressBookScreen::ShownFriendCount() const
{
    return friends.size();
}

std::optional<WiiSpeakScreenName> AddressBookScreen::Press(const std::string& button)
{
    if (button == OkButton)
    {
        session.services.sounds.Request(OkSound);
        return WiiSpeakScreenName::ChatRoom;
    }

    if (button == HelpButton)
    {
        session.services.sounds.Request(HelpSound);
        session.ShowMessage(WiiSpeakText::AddressBookHelpText, {WiiSpeakText::Ok});
    }
    else if (button == AddButton)
    {
        session.services.sounds.Request(AddSound);
        StartAddingAFriend();
    }
    else if (button == LeftButton || button == RightButton)
    {
        bool isRight = button == RightButton;
        std::size_t pageCount = PageCount();
        page = isRight ? (page + 1) % pageCount : (page + pageCount - 1) % pageCount;
        session.services.sounds.Request(isRight ? NextPageSound : PreviousPageSound);
        ShowPage();
    }
    return std::nullopt;
}

void AddressBookScreen::StartAddingAFriend()
{
    if (friends.size() >= MostFriends)
    {
        session.ShowMessage(WiiSpeakText::AddressBookFull, {WiiSpeakText::Ok});
        return;
    }

    typing = Typing::WiiNumber;
    OpenKeyboard(WiiSpeakText::EnterWiiNumber, WiiNumberDigits, true);
}

void AddressBookScreen::TakeTheWiiNumber(const std::u16string& typed)
{
    typing = Typing::Nothing;
    std::optional<std::uint64_t> wiiNumber = ReadWiiNumber(typed);
    if (!wiiNumber || !IsValidWiiNumber(*wiiNumber))
    {
        session.ShowMessage(WiiSpeakText::WiiNumberIncorrect, {WiiSpeakText::Ok});
        return;
    }

    if (*wiiNumber == session.friends.OwnWiiNumber())
    {
        session.ShowMessage(WiiSpeakText::OwnWiiNumberCannotBeRegistered, {WiiSpeakText::Ok});
        return;
    }

    bool isKnown = std::any_of(friends.begin(), friends.end(), [&](const WiiFriend& known) { return known.wiiNumber == *wiiNumber; });
    if (isKnown)
    {
        session.ShowMessage(WiiSpeakText::WiiNumberAlreadyRegistered, {WiiSpeakText::Ok});
        return;
    }

    typedWiiNumber = *wiiNumber;
    typing = Typing::Nickname;
    OpenKeyboard(WiiSpeakText::EnterNickname, NicknameLength, false);
}

void AddressBookScreen::TakeTheNickname(const std::u16string& typed)
{
    typing = Typing::Nothing;
    if (typed.empty())
        return;

    if (session.friends.AddFriend({typedWiiNumber, typed}) == FriendServerAnswer::Unreachable)
    {
        session.ShowWiiConnect24Unreachable();
        return;
    }

    friends = session.friends.Friends();
    ShowPage();
}

void AddressBookScreen::OpenKeyboard(std::uint32_t titleMessageId, std::size_t maximumLength, bool takesDigitsOnly)
{
    SystemMenu::KeyboardRequest request;
    request.title = session.Text(titleMessageId);
    request.rules.maximumLength = maximumLength;
    request.rules.allowedCharacters = takesDigitsOnly ? SystemMenu::IsDigit : SystemMenu::IsAnyCharacter;
    session.keyboard.Open(request);
}

void AddressBookScreen::ShowPage()
{
    for (std::size_t row = 0; row < FriendsPerPage; ++row)
    {
        std::size_t index = page * FriendsPerPage + row;
        bool hasFriend = index < friends.size();
        layout.SetText(Numbered("T_name", row), hasFriend ? friends[index].nickname : u"");
        layout.SetVisible(Numbered("N_box", row), hasFriend);
    }

    std::size_t shownPage = page + 1;
    layout.SetText("T_Pno_00", TensDigit(shownPage));
    layout.SetText("T_Pno_01", OnesDigit(shownPage));
    layout.SetText("T_Cno_00", TensDigit(friends.size()));
    layout.SetText("T_Cno_01", OnesDigit(friends.size()));
}

std::size_t AddressBookScreen::PageCount() const
{
    std::size_t pages = (friends.size() + FriendsPerPage - 1) / FriendsPerPage;
    return std::max<std::size_t>(pages, 1);
}

}
