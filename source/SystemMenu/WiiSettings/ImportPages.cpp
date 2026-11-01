#include "SystemMenu/WiiSettings/ImportPages.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

namespace SystemMenu
{

namespace
{

// The words of the program are plain ASCII, so each character becomes one character of the page.
std::u16string TextOf(const std::string& sentence)
{
    return std::u16string(sentence.begin(), sentence.end());
}

}

ImportPages::ImportPages(PageNavigator& pageNavigator, ConsoleImport& consoleImport)
    : navigator(pageNavigator)
    , import(consoleImport)
{
}

bool ImportPages::Owns(PageId page) const
{
    return page >= PageId::PcImport && page <= PageId::PcImportFailed;
}

void ImportPages::BuildPage(PageId page, Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, u"Import a Wii NAND");

    if (page == PageId::PcImport)
    {
        builder.AddMessage({u"Bring your own Wii into WeeOS: its Miis,", u"saves, channels and Message Board.", u"",
            u"Choose the nand.bin of its BootMii backup.", u"WeeOS starts again with your Wii."});
        builder.AddBackButton([this] { navigator.GoTo(PageId::PcIndex); });
        builder.AddBottomRight(u"Import", [this] { StartImport(); });
        return;
    }

    if (page == PageId::PcImporting)
    {
        builder.AddMessage({u"Importing your Wii...", u"Please wait."});
        return;
    }

    builder.AddMessage({u"The import did not work.", u"", failure});
    builder.AddBottomRight(u"OK", [this] { navigator.GoTo(PageId::PcImport); });
}

// When the import has worked, the program starts again with the imported Wii, so the page only has to watch for a failure.
void ImportPages::Step(PageId shownPage)
{
    if (shownPage != PageId::PcImporting || import.IsImporting())
        return;

    std::optional<std::string> reason = import.TakeFailure();
    if (!reason)
        return;

    failure = TextOf(*reason);
    navigator.GoTo(PageId::PcImportFailed);
}

// A person who closes the picker without a NAND stays on the page.
void ImportPages::StartImport()
{
    if (import.PickAndStartImport())
        navigator.GoTo(PageId::PcImporting);
}

}
