#pragma once

#include "SystemMenu/TitleContents.h"

#include "Ios/Title/TitleId.h"
#include "Ios/Title/TitleService.h"

namespace SystemMenu
{

// The contents of an installed title that is not the one that runs, read through the title service with the title's first ticket.
// A channel reads the dialog windows and the fonts of the System Menu this way for the question "Turn off WeeOS?".
class InstalledTitleContents : public TitleContents
{
public:
    // The title service must stay alive as long as this object.
    InstalledTitleContents(Ios::TitleService& titleService, Ios::TitleId titleId);

    Formats::Bytes ReadContent(std::uint16_t contentIndex) override;

private:
    Ios::TitleService& titleService;
    Ios::TitleId titleId;
};

}
