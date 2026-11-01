#pragma once

#include "SystemMenu/ConsoleImport.h"
#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include <string>

namespace SystemMenu
{

// "Import a Wii NAND" of PC Settings: the import of the person's own Wii, the same import that the first start offers
// (docs/DECISIONS.md, Start, welcome and identity). The program picks the NAND, imports it beside the menu and starts again with the
// imported Wii; the pages ask for it, wait while it works, and tell why it failed when it did.
class ImportPages : public PageGroup
{
public:
    // The navigator and the import must outlive the pages.
    ImportPages(PageNavigator& pageNavigator, ConsoleImport& consoleImport);

    bool Owns(PageId page) const override;
    void BuildPage(PageId page, Page& content) override;
    void Step(PageId shownPage) override;

private:
    void StartImport();

    PageNavigator& navigator;
    ConsoleImport& import;
    std::u16string failure;
};

}
