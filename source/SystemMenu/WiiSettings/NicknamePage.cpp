#include "SystemMenu/WiiSettings/NicknamePage.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

namespace SystemMenu
{

namespace
{

constexpr std::size_t MaximumNicknameLength = 10;

}

NicknamePage::NicknamePage(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings)
    : navigator(pageNavigator)
    , settings(systemSettings)
{
}

bool NicknamePage::Owns(PageId page) const
{
    return page == PageId::NicknameSet;
}

void NicknamePage::EnterPage(PageId page)
{
    static_cast<void>(page);
    nickname = settings.GetNickname();
}

void NicknamePage::BuildPage(PageId page, Page& content)
{
    static_cast<void>(page);

    PageBuilder builder(content);
    builder.SetTitle(1, u"Console Nickname");

    // The field is a button: pressing it opens the keyboard. The page never shows more text than the console can keep.
    builder.AddChoice(0, 1, nickname, false, [this] { AskForNickname(); }).name = "Nickname";

    builder.AddBackButton([this] { navigator.GoTo(PageId::FirstIndex); });
    builder.AddBottomRight(u"Confirm",
        [this]
        {
            settings.SetNickname(nickname);
            settings.Save();
            navigator.GoTo(PageId::FirstIndex);
        });
}

void NicknamePage::AskForNickname()
{
    InputRequest request;
    request.kind = InputKind::Text;
    request.initialText = nickname;
    request.maximumLength = MaximumNicknameLength;

    navigator.RequestInput(request,
        [this](std::optional<std::u16string> typedText)
        {
            if (typedText)
                nickname = typedText->substr(0, MaximumNicknameLength);

            navigator.Refresh();
        });
}

}
