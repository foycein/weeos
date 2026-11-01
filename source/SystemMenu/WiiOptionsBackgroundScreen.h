#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"

#include "Layout/LayoutArchive.h"

#include <memory>

namespace SystemMenu
{

// The background of Wii Options (screen 0x15). It draws the picture behind the other two screens of Wii Options and opens them,
// the bar with the Back button and the screen with the buttons, as its children. The picture fades in from black.
//
// The argument says where the screen with the buttons starts: 0 on its first page, with Data Management and Wii Settings,
// and any other number on the Data Management page. The two children get the same argument.
class WiiOptionsBackgroundScreen : public Screen
{
public:
    WiiOptionsBackgroundScreen(MenuServices& menuServices, int screenArgument);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    // The screen goes on while its children stand there, and is drawn behind them.
    bool UpdatesWithChildren() const override;
    bool DrawsWithChildren() const override;

protected:
    void AfterStep() override;

private:
    MenuServices& services;
    int argument;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
};

}
