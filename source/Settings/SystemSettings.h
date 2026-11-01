#pragma once

#include "Formats/SystemConfiguration.h"
#include "Ios/Caller.h"
#include "Ios/FileSystem/FileSystemService.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Settings
{

enum class AspectRatio : std::uint8_t
{
    FourByThree = 0,
    SixteenByNine = 1,
};

enum class SoundMode : std::uint8_t
{
    Mono = 0,
    Stereo = 1,
    Surround = 2,
};

// Where the sensor bar stands. The remote needs to know to find the screen.
enum class SensorBarPosition : std::uint8_t
{
    BelowTelevision = 0,
    AboveTelevision = 1,
};

// The numbers are the ones the console stores.
enum class Language : std::uint8_t
{
    Japanese = 0,
    English = 1,
    German = 2,
    French = 3,
    Spanish = 4,
    Italian = 5,
    Dutch = 6,
    SimplifiedChinese = 7,
    TraditionalChinese = 8,
    Korean = 9,
};

// The parental controls as the console keeps them in one setting.
struct ParentalControls
{
    bool isOn = false;

    // The organization that rates the games. 1 is the ESRB of the USA.
    std::uint8_t organization = 1;

    // The highest rating that is allowed, written as the youngest age it is meant for. 20 allows everything.
    std::uint8_t highestAllowedAge = 20;

    // Four digits, or nothing when no PIN was set.
    std::string pin;

    // The number of the question that the PIN can be asked back with, and the answer to it.
    std::uint8_t secretQuestion = 0;
    std::u16string secretAnswer;

    bool operator==(const ParentalControls&) const = default;
};

// What the parental controls lock, when they are on. The console keeps three of them in one number and the Internet Channel in a setting of its own.
struct ParentalRestrictions
{
    bool isNewsChannelLocked = false;
    bool isMessageBoardLocked = false;
    bool isShoppingLocked = false;
    bool isInternetChannelLocked = false;

    bool operator==(const ParentalRestrictions&) const = default;
};

// The settings of the console: the file /shared2/sys/SYSCONF, read through the file system of IOS. The System Menu is the one that
// changes them. The values are held in memory; Save writes them back to the file, as the Wii Settings pages do when the user confirms.
//
// Every value has a default for the case that the file does not have the setting at all, and a value that is outside of
// what the console accepts is read as the console reads it (a screen saver of 7 is off, an offset of 5 is 4).
class SystemSettings
{
public:
    // Reads the file. Throws a SettingsError when it cannot be read or is not a settings file.
    // The file system must outlive this object.
    SystemSettings(Ios::FileSystemService& fileSystem, const Ios::Caller& caller);

    // Writes all settings to the file. Throws a SettingsError when that fails.
    void Save();

    bool HasUnsavedChanges() const;

    Language GetLanguage() const;
    void SetLanguage(Language language);

    // The code of the country the console is in (the first number of the location setting). The codes are the console's.
    std::uint8_t GetCountryCode() const;

    // Sets the country and clears the rest of the location (region, city and the names).
    void SetCountryCode(std::uint8_t countryCode);

    AspectRatio GetAspectRatio() const;
    void SetAspectRatio(AspectRatio aspectRatio);

    bool IsProgressiveScanOn() const;
    void SetProgressiveScan(bool isOn);

    bool IsScreenSaverOn() const;
    void SetScreenSaver(bool isOn);

    // How far the picture is moved sideways, from -32 to 32 in steps of 2.
    int GetDisplayOffset() const;
    void SetDisplayOffset(int offset);

    SoundMode GetSoundMode() const;
    void SetSoundMode(SoundMode soundMode);

    SensorBarPosition GetSensorBarPosition() const;
    void SetSensorBarPosition(SensorBarPosition position);

    // How sensitive the pointer is, from 1 to 5.
    int GetPointerSensitivity() const;
    void SetPointerSensitivity(int sensitivity);

    // The name of the console, up to ten characters.
    std::u16string GetNickname() const;
    void SetNickname(const std::u16string& nickname);

    // The seconds that are added to the clock of the PC to get the time of the console.
    std::int32_t GetClockBias() const;
    void SetClockBias(std::int32_t seconds);

    ParentalControls GetParentalControls() const;
    void SetParentalControls(const ParentalControls& parentalControls);

    ParentalRestrictions GetParentalRestrictions() const;
    void SetParentalRestrictions(const ParentalRestrictions& restrictions);

    // The settings as they are held in memory. For tests and for tools that want to see every setting.
    const Formats::SystemConfiguration& Configuration() const;

private:
    Formats::Setting& FindOrAddSetting(std::string_view name);
    void StoreNumber(std::string_view name, Formats::SettingType type, std::uint64_t number);
    void StoreBytes(std::string_view name, Formats::SettingType type, Formats::Bytes bytes);

    Ios::FileSystemService& fileSystem;
    Ios::Caller caller;
    Formats::SystemConfiguration configuration;
    bool hasUnsavedChanges = false;
};

}
