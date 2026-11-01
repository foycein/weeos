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

// The name of a console that has none, as a fresh WeeOS is (docs/DECISIONS.md, Settings).
constexpr std::u16string_view FreshConsoleNickname = u"WeeOS";

// Where the sensor bar stands. The remote needs to know to find the screen.
enum class SensorBarPosition : std::uint8_t
{
    BelowTelevision = 0,
    AboveTelevision = 1,
};

// The settings of the console: the file /shared2/sys/SYSCONF, read through the file system of IOS. The System Menu is the one that
// changes them. The values are held in memory; Save writes them back to the file, as the Wii Settings pages do when the user confirms.
//
// Every value has a default for the case that the file does not have the setting at all, and a value that is outside of
// what the console accepts is read as the console reads it (a sound mode of 7 is stereo, a sensitivity of 9 is 5).
// Date, time, country and language are not settings of WeeOS: they follow the PC (docs/DECISIONS.md, Settings).
class SystemSettings
{
public:
    // Reads the file. Throws a SettingsError when it cannot be read or is not a settings file.
    // The file system must outlive this object.
    SystemSettings(Ios::FileSystemService& fileSystem, const Ios::Caller& caller);

    // Writes all settings to the file. Throws a SettingsError when that fails.
    void Save();

    bool HasUnsavedChanges() const;

    // The code of the country the console is in (the first number of the location setting). The codes are the console's.
    // WeeOS sets it to the country of the PC at every start (Settings::CountryCodeOfThePc).
    std::uint8_t GetCountryCode() const;

    // Sets the country and clears the rest of the location (region, city and the names).
    void SetCountryCode(std::uint8_t countryCode);

    AspectRatio GetAspectRatio() const;
    SoundMode GetSoundMode() const;
    void SetSoundMode(SoundMode soundMode);

    SensorBarPosition GetSensorBarPosition() const;
    void SetSensorBarPosition(SensorBarPosition position);

    // How sensitive the pointer is, from 1 to 5.
    int GetPointerSensitivity() const;
    void SetPointerSensitivity(int sensitivity);

    // The name of the console, up to ten characters. A console without a name is called FreshConsoleNickname.
    std::u16string GetNickname() const;
    void SetNickname(const std::u16string& nickname);

    // Whether the remotes may rumble. The HOME menu and PC Settings change it.
    bool IsRumbleOn() const;
    void SetRumble(bool isOn);

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
