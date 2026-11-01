#include "SystemMenu/WiiSettings/CountryPages.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

#include "Settings/Countries.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

// The first page has four countries and the request, the other pages have five countries.
constexpr int CountriesOnFirstPage = 4;
constexpr int CountriesOnOtherPages = 5;

// The list lies below the tabs. The buttons are 432 by 56 points and follow each other without a gap.
constexpr int ListLeft = 88;
constexpr int ListTop = 76;
constexpr int ItemWidth = 432;
constexpr int ItemHeight = 56;
constexpr int ArrowSize = 72;
constexpr int ArrowLeft = 528;
constexpr int ArrowUpTop = 76;
constexpr int ArrowDownTop = 282;
constexpr PageRectangle RequestBounds = {ListLeft, ListTop, ItemWidth, ItemHeight};

int FirstCountryOfPage(int page)
{
    if (page == 0)
        return 0;

    return CountriesOnFirstPage + (page - 1) * CountriesOnOtherPages;
}

int PageCount()
{
    int countryCount = static_cast<int>(Settings::SelectableCountries().size());
    int otherPagesCount = (countryCount - CountriesOnFirstPage + CountriesOnOtherPages - 1) / CountriesOnOtherPages;
    return 1 + otherPagesCount;
}

}

CountryPages::CountryPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings)
    : navigator(pageNavigator)
    , settings(systemSettings)
{
}

bool CountryPages::Owns(PageId page) const
{
    return page == PageId::CountrySelect;
}

void CountryPages::EnterPage(PageId page)
{
    static_cast<void>(page);
    shownPage = 0;
    chosenCountryCode = settings.GetCountryCode();
}

void CountryPages::BuildPage(PageId page, Page& content)
{
    static_cast<void>(page);

    PageBuilder builder(content);
    builder.SetTitle(1, u"Country");

    std::span<const Settings::Country> countries = Settings::SelectableCountries();
    int firstCountry = FirstCountryOfPage(shownPage);
    int countriesOnPage = shownPage == 0 ? CountriesOnFirstPage : CountriesOnOtherPages;
    int lastCountry = std::min(firstCountry + countriesOnPage, static_cast<int>(countries.size()));

    int itemTop = ListTop;
    if (shownPage == 0)
    {
        builder.AddText(RequestBounds, TextRole::Message, {u"Select your country of residence."});
        itemTop += ItemHeight;
    }

    for (int countryNumber = firstCountry; countryNumber < lastCountry; ++countryNumber)
    {
        const Settings::Country& country = countries[countryNumber];
        PageRectangle bounds = {ListLeft, itemTop, ItemWidth, ItemHeight};
        itemTop += ItemHeight;

        PageButton& item = builder.AddPictureButton(ButtonLook::CountryItem, bounds, NameOfText(country.name),
            [this, code = country.code]
            {
                chosenCountryCode = code;
                navigator.Refresh();
            });
        item.text = std::u16string(country.name);
        item.isChosen = country.code == chosenCountryCode;
        item.sound = PageSound::Choose;
    }

    if (shownPage > 0)
    {
        builder.AddPictureButton(ButtonLook::ArrowUp, {ArrowLeft, ArrowUpTop, ArrowSize, ArrowSize}, "PreviousCountries", [this] { TurnPage(-1); })
            .sound = PageSound::Scroll;
    }

    if (shownPage < PageCount() - 1)
    {
        builder.AddPictureButton(ButtonLook::ArrowDown, {ArrowLeft, ArrowDownTop, ArrowSize, ArrowSize}, "NextCountries", [this] { TurnPage(1); })
            .sound = PageSound::Scroll;
    }

    builder.AddBackButton([this] { navigator.GoTo(PageId::ThirdIndex); });
    builder.AddBottomRight(u"OK",
        [this]
        {
            if (chosenCountryCode != 0)
            {
                settings.SetCountryCode(static_cast<std::uint8_t>(chosenCountryCode));
                settings.Save();
            }

            navigator.GoTo(PageId::ThirdIndex);
        });
}

void CountryPages::TurnPage(int change)
{
    shownPage += change;
    navigator.Refresh();
}

}
