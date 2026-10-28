#pragma once

#include "SystemMenu/TitleContents.h"

#include "Ios/Title/TitleService.h"

namespace SystemMenu
{

// The contents of the title that runs now, read through the title service of IOS.
// The title service must have entered the System Menu before the first read.
class OwnTitleContents : public TitleContents
{
public:
    // The title service must stay alive as long as this object.
    explicit OwnTitleContents(Ios::TitleService& titleService);

    Formats::Bytes ReadContent(std::uint16_t contentIndex) override;

private:
    Ios::TitleService& titleService;
};

}
