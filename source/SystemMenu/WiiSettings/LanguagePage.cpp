#include "SystemMenu/WiiSettings/LanguagePage.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

namespace SystemMenu
{

namespace
{

constexpr int ChoiceCount = 1;

}

LanguagePage::LanguagePage(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings)
    : navigator(pageNavigator)
    , settings(systemSettings)
{
}

bool LanguagePage::Owns(PageId page) const
{
    return page == PageId::LanguageSet;
}

void LanguagePage::BuildPage(PageId page, Page& content)
{
    static_cast<void>(page);

    PageBuilder builder(content);
    builder.SetTitle(1, u"Language");

    builder.AddChoice(0, ChoiceCount, u"English", true, [] {});

    builder.AddBackButton([this] { navigator.GoTo(PageId::ThirdIndex); });
    builder.AddBottomRight(u"Confirm",
        [this]
        {
            settings.SetLanguage(Settings::Language::English);
            settings.Save();
            navigator.GoTo(PageId::ThirdIndex);
        });
}

}
