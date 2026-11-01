#include "Settings/SystemSettings.h"

#include "Settings/SettingsError.h"

#include "Formats/FormatError.h"
#include "Ios/FileSystem/NewEntryDefaults.h"

#include <algorithm>
#include <format>
#include <optional>
#include <utility>

namespace Settings
{

namespace
{

constexpr std::string_view SettingsFilePath = "/shared2/sys/SYSCONF";

// The names of the settings in the file.
constexpr std::string_view AspectRatioName = "IPL.AR";
constexpr std::string_view ClockBiasName = "IPL.CB";
constexpr std::string_view DisplayOffsetName = "IPL.DH";
constexpr std::string_view InternetChannelRestrictionName = "WWW.RST";
constexpr std::string_view LanguageName = "IPL.LNG";
constexpr std::string_view LocationName = "IPL.SADR";
constexpr std::string_view NicknameName = "IPL.NIK";
constexpr std::string_view ParentalControlsName = "IPL.PC";
constexpr std::string_view ParentalRestrictionsName = "NET.CTPC";
constexpr std::string_view PointerSensitivityName = "BT.SENS";
constexpr std::string_view ProgressiveScanName = "IPL.PGS";
constexpr std::string_view ScreenSaverName = "IPL.SSV";
constexpr std::string_view SensorBarPositionName = "BT.BAR";
constexpr std::string_view SoundModeName = "IPL.SND";

// A setting that is a flag is on when it holds 1, and off when it holds any other number.
constexpr std::uint64_t FlagIsOn = 1;

// The console starts with the screen saver on, as the console does when the file has no setting for it.
constexpr bool ScreenSaverIsOnWithoutSetting = true;

constexpr std::uint64_t HighestLanguageNumber = static_cast<std::uint64_t>(Language::Korean);
constexpr int DefaultPointerSensitivity = 2;
constexpr int LowestPointerSensitivity = 1;
constexpr int HighestPointerSensitivity = 5;
constexpr int LargestDisplayOffset = 32;

// A console that has never been asked to change its clock has the clock of the PC.
constexpr std::int32_t ClockBiasWithoutSetting = 0;

// The parental controls are 74 bytes: the flags, the organization, the highest allowed age, four digits of PIN, the number of the
// secret question, the answer (32 characters of 2 bytes) and a number with the length of the answer.
constexpr std::size_t ParentalControlsSize = 74;
constexpr std::size_t ParentalFlagsOffset = 0;
constexpr std::size_t ParentalOrganizationOffset = 1;
constexpr std::size_t ParentalAgeOffset = 2;
constexpr std::size_t ParentalPinOffset = 3;
constexpr std::size_t ParentalPinLength = 4;
constexpr std::size_t ParentalQuestionOffset = 7;
constexpr std::size_t ParentalAnswerOffset = 8;
constexpr std::size_t ParentalAnswerCharacterCount = 32;
constexpr std::size_t ParentalAnswerLengthOffset = 72;
constexpr Formats::Byte ParentalControlsOnFlag = 0x80;

// The nickname is 22 bytes: ten characters of 2 bytes and a number with the length of the name.
constexpr std::size_t NicknameSize = 22;
constexpr std::size_t NicknameCharacterCount = 10;
constexpr std::size_t NicknameLengthOffset = 20;

// The location is 4104 bytes. The first one is the country, the rest is the region, the city, their names and the coordinates.
constexpr std::size_t LocationSize = 4104;
constexpr std::size_t CountryOffset = 0;

// What the three network restrictions are in the one number the console keeps.
constexpr std::uint64_t NewsChannelLockedBit = 0x01;
constexpr std::uint64_t MessageBoardLockedBit = 0x02;
constexpr std::uint64_t ShoppingLockedBit = 0x04;

constexpr std::size_t BytesPerCharacter = 2;

[[noreturn]] void FailWith(const std::string& problem, Ios::IosError error)
{
    throw SettingsError(std::format("{} (result code {})", problem, Ios::OriginalResultCode(error)));
}

Ios::NandPath ParseSettingsFilePath()
{
    Ios::Result<Ios::NandPath> path = Ios::NandPath::Parse(SettingsFilePath);
    if (path.Failed())
        FailWith("The path of the settings file is not accepted", path.Error());

    return path.Value();
}

Formats::Bytes ReadSettingsFile(Ios::FileSystemService& fileSystem, const Ios::Caller& caller)
{
    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(ParseSettingsFilePath(), Ios::OpenMode::Read, caller);
    if (opened.Failed())
        FailWith("The settings file cannot be opened", opened.Error());

    Ios::OpenFile& file = opened.Value();
    Ios::Result<Ios::FileStatistics> statistics = file.GetStatistics();
    if (statistics.Failed())
        FailWith("The size of the settings file cannot be read", statistics.Error());

    Ios::Result<Formats::Bytes> bytes = file.Read(statistics.Value().length);
    if (bytes.Failed())
        FailWith("The settings file cannot be read", bytes.Error());

    return bytes.Value();
}

void MakeSettingsFile(Ios::FileSystemService& fileSystem, const Ios::NandPath& path, const Ios::Caller& caller)
{
    Ios::Result<void> created = fileSystem.CreateFile(path, Ios::ReadWriteForEveryone, Ios::NoEntryFlags, caller);
    if (created.Failed())
        FailWith("The settings file cannot be made", created.Error());
}

void WriteSettingsFile(Ios::FileSystemService& fileSystem, const Ios::Caller& caller, Formats::ByteView bytes)
{
    Ios::NandPath path = ParseSettingsFilePath();

    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(path, Ios::OpenMode::Write, caller);
    if (opened.Failed() && opened.Error() == Ios::IosError::NotFound)
    {
        MakeSettingsFile(fileSystem, path, caller);
        opened = fileSystem.Open(path, Ios::OpenMode::Write, caller);
    }

    if (opened.Failed())
        FailWith("The settings file cannot be opened for writing", opened.Error());

    Ios::Result<std::size_t> written = opened.Value().Write(bytes);
    if (written.Failed())
        FailWith("The settings file cannot be written", written.Error());

    if (written.Value() != bytes.size())
        throw SettingsError(std::format("The settings file was written with {} bytes of {}", written.Value(), bytes.size()));
}

Formats::SystemConfiguration ReadConfiguration(Ios::FileSystemService& fileSystem, const Ios::Caller& caller)
{
    Formats::Bytes bytes = ReadSettingsFile(fileSystem, caller);

    try
    {
        return Formats::ReadSystemConfiguration(bytes);
    }
    catch (const Formats::FormatError& error)
    {
        throw SettingsError(std::format("The settings file is not a settings file: {}", error.what()));
    }
}

bool FlagOf(const Formats::SystemConfiguration& configuration, std::string_view name, bool flagWithoutSetting)
{
    std::optional<std::uint64_t> number = configuration.FindNumber(name);
    if (!number)
        return flagWithoutSetting;

    return *number == FlagIsOn;
}

std::uint64_t FlagNumber(bool isOn)
{
    if (isOn)
        return FlagIsOn;

    return 0;
}

// The array of a setting, when it has the size that the console gives the setting.
std::optional<Formats::ByteView> ArrayOfSize(const Formats::SystemConfiguration& configuration, std::string_view name, std::size_t size)
{
    std::optional<Formats::ByteView> bytes = configuration.FindBytes(name);
    if (!bytes || bytes->size() != size)
        return std::nullopt;

    return bytes;
}

std::uint16_t ReadNumber16(Formats::ByteView bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>((bytes[offset] << Formats::BitsPerByte) | bytes[offset + 1]);
}

void WriteNumber16(Formats::Bytes& bytes, std::size_t offset, std::uint16_t number)
{
    bytes[offset] = static_cast<Formats::Byte>(number >> Formats::BitsPerByte);
    bytes[offset + 1] = static_cast<Formats::Byte>(number & Formats::ByteMask);
}

// Text is kept as characters of two bytes, the first byte first.
std::u16string ReadText(Formats::ByteView bytes, std::size_t offset, std::size_t length, std::size_t characterCount)
{
    std::u16string text;
    std::size_t usedLength = std::min(length, characterCount);
    for (std::size_t characterNumber = 0; characterNumber < usedLength; ++characterNumber)
        text.push_back(static_cast<char16_t>(ReadNumber16(bytes, offset + characterNumber * BytesPerCharacter)));

    return text;
}

void WriteText(Formats::Bytes& bytes, std::size_t offset, const std::u16string& text, std::size_t characterCount)
{
    std::size_t usedLength = std::min(text.size(), characterCount);
    for (std::size_t characterNumber = 0; characterNumber < usedLength; ++characterNumber)
        WriteNumber16(bytes, offset + characterNumber * BytesPerCharacter, static_cast<std::uint16_t>(text[characterNumber]));
}

// An offset that is odd is moved to the even number below it.
int RoundDownToEven(int number)
{
    return number & ~1;
}

int NormalizeDisplayOffset(int offset)
{
    return RoundDownToEven(std::clamp(offset, -LargestDisplayOffset, LargestDisplayOffset));
}

bool IsRestrictionOn(std::uint64_t restrictions, std::uint64_t bit)
{
    return (restrictions & bit) != 0;
}

std::uint64_t RestrictionBit(bool isLocked, std::uint64_t bit)
{
    if (isLocked)
        return bit;

    return 0;
}

}

SystemSettings::SystemSettings(Ios::FileSystemService& fileSystem, const Ios::Caller& caller)
    : fileSystem(fileSystem)
    , caller(caller)
    , configuration(ReadConfiguration(fileSystem, caller))
{
}

void SystemSettings::Save()
{
    Formats::Bytes bytes;
    try
    {
        bytes = Formats::WriteSystemConfiguration(configuration);
    }
    catch (const Formats::FormatError& error)
    {
        throw SettingsError(std::format("The settings cannot be stored: {}", error.what()));
    }

    WriteSettingsFile(fileSystem, caller, bytes);
    hasUnsavedChanges = false;
}

bool SystemSettings::HasUnsavedChanges() const
{
    return hasUnsavedChanges;
}

Language SystemSettings::GetLanguage() const
{
    std::optional<std::uint64_t> number = configuration.FindNumber(LanguageName);
    if (!number || *number > HighestLanguageNumber)
        return Language::English;

    return static_cast<Language>(*number);
}

void SystemSettings::SetLanguage(Language language)
{
    StoreNumber(LanguageName, Formats::SettingType::Byte, static_cast<std::uint64_t>(language));
}

std::uint8_t SystemSettings::GetCountryCode() const
{
    std::optional<Formats::ByteView> location = configuration.FindBytes(LocationName);
    if (!location || location->empty())
        return 0;

    return (*location)[CountryOffset];
}

void SystemSettings::SetCountryCode(std::uint8_t countryCode)
{
    Formats::Bytes location(LocationSize, 0);
    location[CountryOffset] = countryCode;
    StoreBytes(LocationName, Formats::SettingType::BigArray, std::move(location));
}

AspectRatio SystemSettings::GetAspectRatio() const
{
    if (FlagOf(configuration, AspectRatioName, false))
        return AspectRatio::SixteenByNine;

    return AspectRatio::FourByThree;
}

void SystemSettings::SetAspectRatio(AspectRatio aspectRatio)
{
    StoreNumber(AspectRatioName, Formats::SettingType::Byte, static_cast<std::uint64_t>(aspectRatio));
}

bool SystemSettings::IsProgressiveScanOn() const
{
    return FlagOf(configuration, ProgressiveScanName, false);
}

void SystemSettings::SetProgressiveScan(bool isOn)
{
    StoreNumber(ProgressiveScanName, Formats::SettingType::Byte, FlagNumber(isOn));
}

bool SystemSettings::IsScreenSaverOn() const
{
    return FlagOf(configuration, ScreenSaverName, ScreenSaverIsOnWithoutSetting);
}

void SystemSettings::SetScreenSaver(bool isOn)
{
    StoreNumber(ScreenSaverName, Formats::SettingType::Byte, FlagNumber(isOn));
}

int SystemSettings::GetDisplayOffset() const
{
    std::optional<std::uint64_t> number = configuration.FindNumber(DisplayOffsetName);
    if (!number)
        return 0;

    return NormalizeDisplayOffset(static_cast<std::int8_t>(*number));
}

void SystemSettings::SetDisplayOffset(int offset)
{
    std::int8_t storedOffset = static_cast<std::int8_t>(NormalizeDisplayOffset(offset));
    StoreNumber(DisplayOffsetName, Formats::SettingType::Byte, static_cast<Formats::Byte>(storedOffset));
}

SoundMode SystemSettings::GetSoundMode() const
{
    std::optional<std::uint64_t> number = configuration.FindNumber(SoundModeName);
    if (!number || *number > static_cast<std::uint64_t>(SoundMode::Surround))
        return SoundMode::Stereo;

    return static_cast<SoundMode>(*number);
}

void SystemSettings::SetSoundMode(SoundMode soundMode)
{
    StoreNumber(SoundModeName, Formats::SettingType::Byte, static_cast<std::uint64_t>(soundMode));
}

SensorBarPosition SystemSettings::GetSensorBarPosition() const
{
    if (FlagOf(configuration, SensorBarPositionName, false))
        return SensorBarPosition::AboveTelevision;

    return SensorBarPosition::BelowTelevision;
}

void SystemSettings::SetSensorBarPosition(SensorBarPosition position)
{
    StoreNumber(SensorBarPositionName, Formats::SettingType::Byte, static_cast<std::uint64_t>(position));
}

int SystemSettings::GetPointerSensitivity() const
{
    std::optional<std::uint64_t> number = configuration.FindNumber(PointerSensitivityName);
    if (!number)
        return DefaultPointerSensitivity;

    int sensitivity = static_cast<int>(std::min<std::uint64_t>(*number, HighestPointerSensitivity));
    return std::max(sensitivity, LowestPointerSensitivity);
}

void SystemSettings::SetPointerSensitivity(int sensitivity)
{
    int storedSensitivity = std::clamp(sensitivity, LowestPointerSensitivity, HighestPointerSensitivity);
    StoreNumber(PointerSensitivityName, Formats::SettingType::Long, static_cast<std::uint64_t>(storedSensitivity));
}

std::u16string SystemSettings::GetNickname() const
{
    std::optional<Formats::ByteView> bytes = ArrayOfSize(configuration, NicknameName, NicknameSize);
    if (!bytes)
        return {};

    return ReadText(*bytes, 0, ReadNumber16(*bytes, NicknameLengthOffset), NicknameCharacterCount);
}

void SystemSettings::SetNickname(const std::u16string& nickname)
{
    Formats::Bytes bytes(NicknameSize, 0);
    WriteText(bytes, 0, nickname, NicknameCharacterCount);
    WriteNumber16(bytes, NicknameLengthOffset, static_cast<std::uint16_t>(std::min(nickname.size(), NicknameCharacterCount)));
    StoreBytes(NicknameName, Formats::SettingType::SmallArray, std::move(bytes));
}

std::int32_t SystemSettings::GetClockBias() const
{
    std::optional<std::uint64_t> number = configuration.FindNumber(ClockBiasName);
    if (!number)
        return ClockBiasWithoutSetting;

    return static_cast<std::int32_t>(static_cast<std::uint32_t>(*number));
}

void SystemSettings::SetClockBias(std::int32_t seconds)
{
    StoreNumber(ClockBiasName, Formats::SettingType::Long, static_cast<std::uint32_t>(seconds));
}

ParentalControls SystemSettings::GetParentalControls() const
{
    ParentalControls parentalControls;
    std::optional<Formats::ByteView> bytes = ArrayOfSize(configuration, ParentalControlsName, ParentalControlsSize);
    if (!bytes)
        return parentalControls;

    parentalControls.isOn = ((*bytes)[ParentalFlagsOffset] & ParentalControlsOnFlag) != 0;
    parentalControls.organization = (*bytes)[ParentalOrganizationOffset];
    parentalControls.highestAllowedAge = (*bytes)[ParentalAgeOffset];
    parentalControls.pin = std::string(reinterpret_cast<const char*>(bytes->data() + ParentalPinOffset), ParentalPinLength);
    parentalControls.secretQuestion = (*bytes)[ParentalQuestionOffset];
    parentalControls.secretAnswer =
        ReadText(*bytes, ParentalAnswerOffset, ReadNumber16(*bytes, ParentalAnswerLengthOffset), ParentalAnswerCharacterCount);

    // A console without a PIN has four bytes of zero where the digits are.
    if (parentalControls.pin.front() == '\0')
        parentalControls.pin.clear();

    return parentalControls;
}

void SystemSettings::SetParentalControls(const ParentalControls& parentalControls)
{
    Formats::Bytes bytes(ParentalControlsSize, 0);
    if (parentalControls.isOn)
        bytes[ParentalFlagsOffset] = ParentalControlsOnFlag;

    bytes[ParentalOrganizationOffset] = parentalControls.organization;
    bytes[ParentalAgeOffset] = parentalControls.highestAllowedAge;

    std::size_t pinLength = std::min(parentalControls.pin.size(), ParentalPinLength);
    std::copy_n(parentalControls.pin.begin(), pinLength, bytes.begin() + ParentalPinOffset);

    bytes[ParentalQuestionOffset] = parentalControls.secretQuestion;
    WriteText(bytes, ParentalAnswerOffset, parentalControls.secretAnswer, ParentalAnswerCharacterCount);
    std::size_t answerLength = std::min(parentalControls.secretAnswer.size(), ParentalAnswerCharacterCount);
    WriteNumber16(bytes, ParentalAnswerLengthOffset, static_cast<std::uint16_t>(answerLength));

    StoreBytes(ParentalControlsName, Formats::SettingType::SmallArray, std::move(bytes));
}

ParentalRestrictions SystemSettings::GetParentalRestrictions() const
{
    std::uint64_t restrictions = configuration.FindNumber(ParentalRestrictionsName).value_or(0);

    ParentalRestrictions parentalRestrictions;
    parentalRestrictions.isNewsChannelLocked = IsRestrictionOn(restrictions, NewsChannelLockedBit);
    parentalRestrictions.isMessageBoardLocked = IsRestrictionOn(restrictions, MessageBoardLockedBit);
    parentalRestrictions.isShoppingLocked = IsRestrictionOn(restrictions, ShoppingLockedBit);
    parentalRestrictions.isInternetChannelLocked = FlagOf(configuration, InternetChannelRestrictionName, false);
    return parentalRestrictions;
}

void SystemSettings::SetParentalRestrictions(const ParentalRestrictions& restrictions)
{
    std::uint64_t lockedBits = RestrictionBit(restrictions.isNewsChannelLocked, NewsChannelLockedBit)
        | RestrictionBit(restrictions.isMessageBoardLocked, MessageBoardLockedBit)
        | RestrictionBit(restrictions.isShoppingLocked, ShoppingLockedBit);

    StoreNumber(ParentalRestrictionsName, Formats::SettingType::Long, lockedBits);
    StoreNumber(InternetChannelRestrictionName, Formats::SettingType::Bool, FlagNumber(restrictions.isInternetChannelLocked));
}

const Formats::SystemConfiguration& SystemSettings::Configuration() const
{
    return configuration;
}

Formats::Setting& SystemSettings::FindOrAddSetting(std::string_view name)
{
    for (Formats::Setting& setting : configuration.settings)
    {
        if (setting.name == name)
            return setting;
    }

    configuration.settings.push_back(Formats::Setting{std::string(name), Formats::SettingType::Byte, 0, {}});
    return configuration.settings.back();
}

void SystemSettings::StoreNumber(std::string_view name, Formats::SettingType type, std::uint64_t number)
{
    Formats::Setting& setting = FindOrAddSetting(name);
    setting.type = type;
    setting.number = number;
    setting.bytes.clear();
    hasUnsavedChanges = true;
}

void SystemSettings::StoreBytes(std::string_view name, Formats::SettingType type, Formats::Bytes bytes)
{
    Formats::Setting& setting = FindOrAddSetting(name);
    setting.type = type;
    setting.number = 0;
    setting.bytes = std::move(bytes);
    hasUnsavedChanges = true;
}

}
