#pragma once

#include <optional>
#include <string>

namespace SystemMenu
{

// The import of a person's own Wii (docs/DECISIONS.md, Start): the page of the first start and the page of PC Settings ask the program
// for it. The program lets the person pick the NAND of the Wii (nand.bin of a BootMii backup, with its keys), imports everything of that
// console while the page shows that it works, and then starts again with the imported Wii in place of the console that ran.
class ConsoleImport
{
public:
    virtual ~ConsoleImport() = default;

    // True once, on the first start of a new console: the menu then offers the import before Health and Safety.
    virtual bool TakeOfferAtStart() = 0;

    // Lets the person pick the NAND and starts the import in the background. False when nothing was picked.
    virtual bool PickAndStartImport() = 0;

    // True from the start of an import until the program starts the imported console, or until the import failed.
    virtual bool IsImporting() const = 0;

    // Why the last import failed, in a sentence for the person. Given once; nothing when there is no new failure.
    virtual std::optional<std::string> TakeFailure() = 0;
};

}
