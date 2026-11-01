#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"

#include "Layout/LayoutArchive.h"

#include <memory>

namespace SystemMenu
{

// The picture that shows while the menu starts after a title has ended (screen 0x1e): the Wii logo, tile by tile, on black.
// Its layout is not in a file of the title but inside the main program. When the menu is ready the picture fades out and the Wii Menu
// is asked for. WeeOS has no shared files to wait for, so it waits until the picture has faded in.
class BackToWiiMenuScreen : public Screen
{
public:
    explicit BackToWiiMenuScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

protected:
    void BeforeStep() override;
    bool RunMain() override;
    void OnRunEnded() override;
    bool RunClosing() override;

private:
    MenuServices& services;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
};

}
