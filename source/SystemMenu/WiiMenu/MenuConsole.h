#pragma once

#include "Formats/Bytes.h"
#include "Formats/PlayRecordFile.h"
#include "Ios/Title/TitleId.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// A title on the NAND, with what the channel grid needs to know about it.
struct InstalledTitle
{
    Ios::TitleId id;
    std::uint16_t version = 0;

    // The size of the first content, which holds the banner. A title that is only a stub has 64 bytes there.
    std::uint64_t bannerSize = 0;

    // True when the title has a ticket and all its contents are on the NAND.
    bool isInstalledProperly = false;

    // The two letters of the maker, as the title metadata names its group: "01" for Nintendo.
    std::uint16_t groupId = 0;
};

// The date and the time of the place where the console is, as the clock of the menu shows them.
struct LocalTime
{
    int year = 2000;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;
};

// How the clock of the menu writes the time of day: the hours of 12 with AM and PM, as a console of the USA does, or the hours of 24,
// with or without a 0 in front of the hours below 10. WeeOS follows the PC.
struct ClockFormat
{
    bool usesTwentyFourHours = false;
    bool showsLeadingZeroOfHours = false;
};

// What the menu asks of the console that it runs on, besides its own title: the titles on the NAND, the files of the menu
// on the NAND and the clock. The original asks the system libraries ES, NAND and SC and the OS for these.
class MenuConsole
{
public:
    MenuConsole() = default;
    MenuConsole(const MenuConsole&) = delete;
    MenuConsole& operator=(const MenuConsole&) = delete;
    virtual ~MenuConsole() = default;

    // Every title that is installed, in the order of the title service.
    virtual std::vector<InstalledTitle> ListInstalledTitles() = 0;

    // The first content of a title, which is its banner. Throws a MenuError when the title cannot be read.
    virtual Formats::Bytes ReadBannerContent(Ios::TitleId titleId) = 0;

    // A file of the NAND, for example "/shared2/sys/SYSCONF". Gives nothing when the file is not there.
    // Throws a MenuError when the file is there and cannot be read.
    virtual std::optional<Formats::Bytes> ReadNandFile(std::string_view path) = 0;

    // Replaces a file of the NAND or makes it. Throws a MenuError when the NAND cannot take it.
    virtual void WriteNandFile(std::string_view path, Formats::ByteView bytes) = 0;

    virtual LocalTime GetLocalTime() = 0;

    virtual ClockFormat GetClockFormat()
    {
        return ClockFormat();
    }

    // Writes the record of the title that starts now, play_rec.dat (the call 0x81536DC4 of the channel start window).
    virtual void WritePlayRecord(const Formats::PlayRecord& record) = 0;

    // Brings /shared2/sys/flags.dat up to date with the settings, as the channel grid of the original does when it is built
    // (CSFlags::UpdateFlagsFile, 0x81368650).
    virtual void UpdateSharedFlags() = 0;
};

}
