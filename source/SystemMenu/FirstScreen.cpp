#include "SystemMenu/FirstScreen.h"

#include <cstddef>
#include <cstdint>

namespace SystemMenu
{

namespace
{

// The pages of the settings that a console that is not set up opens: the first page of the setup, or the page where a setup that
// was updated goes on.
constexpr int FirstSetupPage = 2;
constexpr int ResumedSetupPage = 5;
constexpr int FirstUpdateTypeThatResumes = 1;
constexpr int LastUpdateTypeThatResumes = 2;

// The file of the System Menu that remembers how the last title ended. Its bytes 4 to 7 are: flags, the type of the state,
// the state of the disc and where to return to.
constexpr std::string_view StateFilePath = "/title/00000001/00000002/data/state.dat";
constexpr std::size_t StateTypeOffset = 5;
constexpr std::size_t StateReturnToOffset = 7;
constexpr std::size_t StateFileMinimumSize = 8;
constexpr std::uint8_t NoStateType = 0xFF;
constexpr std::uint8_t ReturnStateType = 3;
constexpr std::uint8_t ReturnToSettingsCode = 1;
constexpr std::uint8_t ReturnWithArgumentsCode = 2;

// The file that a title writes when it starts the menu with arguments. Its byte 0xB is the kind of the application, its word at 0xC
// says what the title wants. The checksum of the file is not checked.
constexpr std::string_view BootInformationPath = "/shared2/sys/NANDBOOTINFO";
constexpr std::size_t ApplicationTypeOffset = 0xB;
constexpr std::size_t LaunchCodeOffset = 0xC;
constexpr std::size_t LaunchCodeSize = 4;
constexpr std::size_t BootInformationMinimumSize = 0x10;
constexpr std::uint8_t MenuArgumentsApplicationType = 4;

// The record of a title that ran out of time: 32 bytes.
constexpr std::string_view ExpiredRecordPath = "/shared2/expired";
constexpr std::size_t ExpiredRecordSize = 32;

struct LaunchInformation
{
    bool hasArguments = false;
    int launchCode = 0;
};

LaunchInformation ReadLaunchInformation(NandFiles& nand)
{
    std::optional<Formats::Bytes> bytes = nand.ReadFile(BootInformationPath);
    if (!bytes || bytes->size() < BootInformationMinimumSize)
        return {};

    LaunchInformation information;
    information.hasArguments = (*bytes)[ApplicationTypeOffset] == MenuArgumentsApplicationType;
    for (std::size_t index = 0; index < LaunchCodeSize; ++index)
        information.launchCode = information.launchCode * 256 + (*bytes)[LaunchCodeOffset + index];

    return information;
}

void ReadBootType(NandFiles& nand, StartFacts& facts)
{
    std::optional<Formats::Bytes> state = nand.ReadFile(StateFilePath);
    if (!state || state->size() < StateFileMinimumSize)
        return;

    if ((*state)[StateTypeOffset] != ReturnStateType)
        return;

    facts.bootType = BootType::ReturnFromTitle;

    std::uint8_t returnTo = (*state)[StateReturnToOffset];
    if (returnTo == ReturnToSettingsCode)
    {
        facts.bootType = BootType::ReturnToSettings;
        return;
    }

    if (returnTo != ReturnWithArgumentsCode)
        return;

    LaunchInformation launch = ReadLaunchInformation(nand);
    if (launch.hasArguments)
    {
        facts.bootType = BootType::LaunchWithArguments;
        facts.launchCode = launch.launchCode;
    }
}

// The record counts only when the menu starts after a title. At any other start the file is deleted.
bool TakeExpiredRecord(NandFiles& nand, BootType bootType)
{
    if (bootType != BootType::ReturnFromTitle)
    {
        nand.DeleteFile(ExpiredRecordPath);
        return false;
    }

    std::optional<Formats::Bytes> record = nand.ReadFile(ExpiredRecordPath);
    return record && record->size() >= ExpiredRecordSize;
}

FirstScreen ChooseByBootType(const StartFacts& facts)
{
    switch (facts.bootType)
    {
    case BootType::ColdStart:
        return {ScreenId::HealthAndSafety, 0};

    case BootType::ReturnFromTitle:
        return {ScreenId::BackToWiiMenu, 0};

    case BootType::ReturnToSettings:
        return {ScreenId::StartupRouter, RouterToWiiOptions};

    case BootType::LaunchWithArguments:
        break;
    }

    if (facts.launchCode == 1)
        return {ScreenId::StartupRouter, RouterToSettingsCountryPage};

    if (facts.launchCode == 2)
        return {ScreenId::StartupRouter, RouterToWiiOptions};

    return {ScreenId::HealthAndSafety, 0};
}

}

FirstScreen ChooseFirstScreen(const StartFacts& facts)
{
    if (!facts.isConsoleSetUp)
    {
        bool resumes = facts.updateType >= FirstUpdateTypeThatResumes && facts.updateType <= LastUpdateTypeThatResumes;
        return {ScreenId::WiiSettings, resumes ? ResumedSetupPage : FirstSetupPage};
    }

    if (facts.hasExpiredTitle)
        return {ScreenId::LimitOver, 0};

    return ChooseByBootType(facts);
}

StartFacts ReadStartFacts(NandFiles& nand, MenuSettings& settings)
{
    StartFacts facts;
    facts.isConsoleSetUp = settings.IsConsoleSetUp();
    facts.updateType = settings.UpdateType();
    ReadBootType(nand, facts);

    if (facts.isConsoleSetUp)
        facts.hasExpiredTitle = TakeExpiredRecord(nand, facts.bootType);

    return facts;
}

}
