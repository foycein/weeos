#include "SystemMenu/WiiMenu/ChannelLaunch.h"

#include "SystemMenu/WiiMenu/ChannelTitleIds.h"

#include <chrono>

namespace SystemMenu
{

namespace
{

constexpr std::string_view AnimationsFolder = "arc/anim/";
constexpr std::string_view AnimationExtension = ".brlan";
constexpr std::string_view StartAnimation = "banner_Start";
constexpr std::string_view LoopAnimation = "banner_Loop";
constexpr std::string_view WholeAnimation = "banner";
constexpr std::string_view FirstScriptAnimation = "banner_Rso0";

constexpr std::uint64_t TimerTicksPerSecond = 243000000 / 4;
constexpr std::int64_t SecondsPerMinute = 60;
constexpr std::int64_t SecondsPerHour = 3600;
constexpr std::int64_t SecondsPerDay = 86400;
constexpr int FirstYearOfTheTimer = 2000;

bool HasAnimation(const Formats::U8Archive& files, std::string_view animationName)
{
    std::string path = std::string(AnimationsFolder) + std::string(animationName) + std::string(AnimationExtension);
    return files.FindFile(path).has_value();
}

void CopyLine(const std::u16string& line, Formats::PlayRecord& record, std::size_t firstCharacter)
{
    for (std::size_t index = 0; index < line.size() && index < Formats::PlayRecordNameLineLength; ++index)
        record.name[firstCharacter + index] = line[index];
}

}

std::optional<InstalledTitle> FindInstalledTitle(MenuConsole& console, Ios::TitleId titleId)
{
    for (const InstalledTitle& title : console.ListInstalledTitles())
    {
        if (title.id == titleId)
            return title;
    }
    return std::nullopt;
}

BannerAnimations ChooseBannerAnimations(const Formats::U8Archive& bannerFiles, bool isBannerDrivenByCode)
{
    BannerAnimations animations;
    if (HasAnimation(bannerFiles, StartAnimation))
        animations.start = std::string(StartAnimation);

    if (isBannerDrivenByCode && HasAnimation(bannerFiles, FirstScriptAnimation))
    {
        animations.loop = std::string(FirstScriptAnimation);
        animations.loopsOnTheScriptGroup = true;
        return animations;
    }

    if (HasAnimation(bannerFiles, LoopAnimation))
        animations.loop = std::string(LoopAnimation);
    else if (HasAnimation(bannerFiles, WholeAnimation))
        animations.loop = std::string(WholeAnimation);

    return animations;
}

std::uint64_t TimerTicksAt(const LocalTime& time)
{
    using namespace std::chrono;
    year_month_day date{year{time.year}, month{static_cast<unsigned>(time.month)}, day{static_cast<unsigned>(time.day)}};
    year_month_day startOfTheTimer{year{FirstYearOfTheTimer}, January, day{1}};
    std::int64_t days = (sys_days{date} - sys_days{startOfTheTimer}).count();
    std::int64_t seconds = days * SecondsPerDay + time.hour * SecondsPerHour + time.minute * SecondsPerMinute + time.second;
    return static_cast<std::uint64_t>(seconds) * TimerTicksPerSecond;
}

Formats::PlayRecord MakePlayRecord(const Formats::ChannelName& name, Ios::TitleId titleId, std::uint16_t makerCode, const LocalTime& time)
{
    Formats::PlayRecord record = {};
    CopyLine(name.firstLine, record, 0);
    CopyLine(name.secondLine, record, Formats::PlayRecordNameLineLength);
    record.startTime = TimerTicksAt(time);
    record.lastTime = record.startTime;

    for (std::size_t index = 0; index < Formats::PlayRecordGameCodeSize; ++index)
    {
        std::size_t shift = 8 * (Formats::PlayRecordGameCodeSize - 1 - index);
        record.gameCode[index] = static_cast<Formats::Byte>(titleId.lowPart >> shift);
    }

    record.makerCode[0] = static_cast<Formats::Byte>(makerCode >> 8);
    record.makerCode[1] = static_cast<Formats::Byte>(makerCode);
    return record;
}

}
