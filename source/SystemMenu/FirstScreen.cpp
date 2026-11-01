#include "SystemMenu/FirstScreen.h"

#include "Formats/FormatError.h"
#include "Formats/NandBootInfoFile.h"
#include "Formats/StateFile.h"

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

// The file of the System Menu that remembers how the last title ended, and the file with which a title starts the menu with arguments.
constexpr std::string_view StateFilePath = "/title/00000001/00000002/data/state.dat";
constexpr std::string_view BootInformationPath = "/shared2/sys/NANDBOOTINFO";

// The record of a title that ran out of time: 32 bytes.
constexpr std::string_view ExpiredRecordPath = "/shared2/expired";
constexpr std::size_t ExpiredRecordSize = 32;

// A file that is missing or damaged is as good as no file.
std::optional<Formats::StateFile> ReadState(NandFiles& nand)
{
    std::optional<Formats::Bytes> bytes = nand.ReadFile(StateFilePath);
    if (!bytes)
        return std::nullopt;

    try
    {
        return Formats::ReadStateFile(*bytes);
    }
    catch (const Formats::FormatError&)
    {
        return std::nullopt;
    }
}

std::optional<Formats::NandBootInfo> ReadBootInformation(NandFiles& nand)
{
    std::optional<Formats::Bytes> bytes = nand.ReadFile(BootInformationPath);
    if (!bytes)
        return std::nullopt;

    try
    {
        return Formats::ReadNandBootInfo(*bytes);
    }
    catch (const Formats::FormatError&)
    {
        return std::nullopt;
    }
}

void ReadBootType(NandFiles& nand, StartFacts& facts)
{
    std::optional<Formats::StateFile> state = ReadState(nand);
    if (!state || state->type != Formats::StateTypeReturnToMenu)
        return;

    facts.bootType = BootType::ReturnFromTitle;

    if (state->returnTo == Formats::StateReturnToSettings)
    {
        facts.bootType = BootType::ReturnToSettings;
        return;
    }

    if (state->returnTo != Formats::StateReturnWithArguments)
        return;

    std::optional<Formats::NandBootInfo> bootInformation = ReadBootInformation(nand);
    if (!bootInformation || bootInformation->titleType != Formats::NandBootInfoTypeMenuLaunched)
        return;

    facts.bootType = BootType::LaunchWithArguments;
    facts.launchCode = static_cast<int>(bootInformation->launchCode);
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
