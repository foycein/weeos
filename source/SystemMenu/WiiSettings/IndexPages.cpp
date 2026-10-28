#include "SystemMenu/WiiSettings/IndexPages.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

namespace SystemMenu
{

namespace
{

constexpr int PageCount = 3;

}

IndexPages::IndexPages(PageNavigator& pageNavigator)
    : navigator(pageNavigator)
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

    builder.AddMenuItem(0, u"Sensor Bar", [this] { navigator.GoTo(PageId::SensorBarIndex); });

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

    builder.AddPageArrow(ButtonLook::ArrowLeft, [this] { navigator.GoTo(PageId::SecondIndex); });
    builder.AddBackButton([this] { navigator.LeaveSettings(); });
}

}
