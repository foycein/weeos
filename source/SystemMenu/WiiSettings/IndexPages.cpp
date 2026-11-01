#include "SystemMenu/WiiSettings/IndexPages.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

namespace SystemMenu
{

namespace
{

constexpr int PageCount = 3;

}

IndexPages::IndexPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings)
    : navigator(pageNavigator)
    , settings(systemSettings)
{
}

bool IndexPages::Owns(PageId page) const
{
    return page == PageId::FirstIndex || page == PageId::SecondIndex || page == PageId::ThirdIndex;
}

void IndexPages::BuildPage(PageId page, Page& content)
{
    if (page == PageId::FirstIndex)
        BuildFirstIndex(content);
    else if (page == PageId::SecondIndex)
        BuildSecondIndex(content);
    else
        BuildThirdIndex(content);
}

void IndexPages::BuildFirstIndex(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(0, u"Wii System Settings 1");
    builder.SetPageNumbers(PageCount, 1);

    builder.AddMenuItem(0, u"Console Nickname", [this] { navigator.GoTo(PageId::NicknameSet); });
    builder.AddMenuItem(1, u"Calendar", [this] { navigator.GoTo(PageId::CalendarIndex); });
    builder.AddMenuItem(2, u"Screen", [this] { navigator.GoTo(PageId::DisplayIndex); });
    builder.AddMenuItem(3, u"Sound", [this] { navigator.GoTo(PageId::SoundSet); });

    builder.AddPageArrow(ButtonLook::ArrowRight, [this] { navigator.GoTo(PageId::SecondIndex); });
    builder.AddBackButton([this] { navigator.LeaveSettings(); });
}

void IndexPages::BuildSecondIndex(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(0, u"Wii System Settings 2");
    builder.SetPageNumbers(PageCount, 2);

    builder.AddMenuItem(0, u"Parental Controls", [this] { OpenParentalPages(); });
    builder.AddMenuItem(1, u"Sensor Bar", [this] { navigator.GoTo(PageId::SensorBarIndex); });
    builder.AddMenuItem(2, u"Internet", [this] { OpenRestrictedPage(PageId::InternetIndex); });
    builder.AddMenuItem(3, u"PC Settings", [this] { navigator.GoTo(PageId::PcIndex); });

    builder.AddPageArrow(ButtonLook::ArrowLeft, [this] { navigator.GoTo(PageId::FirstIndex); });
    builder.AddPageArrow(ButtonLook::ArrowRight, [this] { navigator.GoTo(PageId::ThirdIndex); });
    builder.AddBackButton([this] { navigator.LeaveSettings(); });
}

void IndexPages::BuildThirdIndex(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(0, u"Wii System Settings 3");
    builder.SetPageNumbers(PageCount, 3);

    builder.AddMenuItem(0, u"Language", [this] { navigator.GoTo(PageId::LanguageSet); });
    builder.AddMenuItem(1, u"Country", [this] { OpenRestrictedPage(PageId::CountrySelect); });
    builder.AddMenuItem(2, u"Wii System Update", [this] { OpenRestrictedPage(PageId::UpdateIndex); });
    builder.AddMenuItem(3, u"Format Wii System Memory", [this] { OpenRestrictedPage(PageId::FormatWarning); });

    builder.AddPageArrow(ButtonLook::ArrowLeft, [this] { navigator.GoTo(PageId::SecondIndex); });
    builder.AddBackButton([this] { navigator.LeaveSettings(); });
}

// The pages that set up the parental controls, or the question whether to change them when they are on.
void IndexPages::OpenParentalPages()
{
    if (settings.GetParentalControls().isOn)
        navigator.GoTo(PageId::ParentalChangeQuestion);
    else
        navigator.GoTo(PageId::ParentalUseQuestion);
}

// The parental controls dialog asks for the PIN when the controls are on (the function 29 of the pages of the original, which opens
// screen 0x1b). A user who cancels it stays on the index page.
void IndexPages::OpenRestrictedPage(PageId page)
{
    if (!settings.GetParentalControls().isOn)
    {
        navigator.GoTo(page);
        return;
    }

    InputRequest request;
    request.kind = InputKind::ParentalPassword;
    navigator.RequestInput(request,
        [this, page](std::optional<std::u16string> answer)
        {
            if (answer)
                navigator.GoTo(page);
        });
}

}
