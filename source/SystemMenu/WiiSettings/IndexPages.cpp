#include "SystemMenu/WiiSettings/IndexPages.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

namespace SystemMenu
{

IndexPages::IndexPages(PageNavigator& pageNavigator)
    : navigator(pageNavigator)
{
}

bool IndexPages::Owns(PageId page) const
{
    return page == PageId::Index;
}

void IndexPages::BuildPage(PageId page, Page& content)
{
    static_cast<void>(page);

    PageBuilder builder(content);
    builder.SetTitle(0, u"Wii System Settings");

    builder.AddMenuItem(0, u"Console Nickname", [this] { navigator.GoTo(PageId::NicknameSet); });
    builder.AddMenuItem(1, u"Sensor Bar", [this] { navigator.GoTo(PageId::SensorBarIndex); });
    builder.AddMenuItem(2, u"Internet", [this] { navigator.GoTo(PageId::ConnectionStatus); });
    builder.AddMenuItem(3, u"PC Settings", [this] { navigator.GoTo(PageId::PcIndex); });

    builder.AddBackButton([this] { navigator.LeaveSettings(); });
}

}
