#include "Settings/PcSettings.h"

#include "Settings/PcKeys.h"
#include "Settings/SettingsError.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <sstream>

namespace Settings
{

namespace
{

constexpr std::string_view WindowWidthName = "WindowWidth";
constexpr std::string_view WindowHeightName = "WindowHeight";
constexpr std::string_view WindowLeftName = "WindowLeft";
constexpr std::string_view WindowTopName = "WindowTop";
constexpr std::string_view FullscreenName = "Fullscreen";
constexpr std::string_view AudioDeviceName = "AudioDevice";
constexpr std::string_view VolumeName = "Volume";
constexpr std::string_view MouseButtonsSwappedName = "MouseButtonsSwapped";
constexpr std::string_view ServerAddressName = "Server";

// The key of a button is the line "Key <name of the button>", for example "Key HOME = Home".
constexpr std::string_view KeyNamePrefix = "Key ";

constexpr std::string_view YesText = "yes";
constexpr std::string_view NoText = "no";

// A window smaller than this cannot show the menu, a larger one no monitor has.
constexpr std::uint32_t SmallestWindowSide = 240;
constexpr std::uint32_t LargestWindowSide = 8192;

constexpr std::string_view FileHeading = "# The settings of WeeOS on this PC. Wii Settings changes them; each line is \"Name = value\".\n";

// UTF-8 keeps a character below 0x80 in one byte, one below 0x800 in two bytes and the others in three. The first byte says how
// many bytes follow, each following byte carries six bits of the character.
constexpr unsigned int LargestOneByteCharacter = 0x7F;
constexpr unsigned int LargestTwoByteCharacter = 0x7FF;
constexpr unsigned int FirstOfTwoMark = 0xC0;
constexpr unsigned int FirstOfThreeMark = 0xE0;
constexpr unsigned int FollowingByteMark = 0x80;
constexpr unsigned int SixBits = 0x3F;
constexpr unsigned int BitsPerFollowingByte = 6;
constexpr unsigned int FirstOfTwoPatternMask = 0xE0;
constexpr unsigned int FirstOfThreePatternMask = 0xF0;
constexpr unsigned int FirstOfTwoValueBits = 0x1F;
constexpr unsigned int FirstOfThreeValueBits = 0x0F;

char FollowingByte(unsigned int bits)
{
    return static_cast<char>(FollowingByteMark | (bits & SixBits));
}

unsigned int BitsOfFollowingByte(char byte)
{
    return static_cast<unsigned char>(byte) & SixBits;
}

// shortcut: a character outside of the first 65536 is written as its two halves, each as its own UTF-8 sequence; enough for
// the names of sound devices, upgrade to real surrogate pairs if a name ever needs it.
std::string Utf8FromText(std::u16string_view text)
{
    std::string utf8;
    for (char16_t character : text)
    {
        unsigned int code = character;
        if (code <= LargestOneByteCharacter)
        {
            utf8.push_back(static_cast<char>(code));
        }
        else if (code <= LargestTwoByteCharacter)
        {
            utf8.push_back(static_cast<char>(FirstOfTwoMark | (code >> BitsPerFollowingByte)));
            utf8.push_back(FollowingByte(code));
        }
        else
        {
            utf8.push_back(static_cast<char>(FirstOfThreeMark | (code >> (2 * BitsPerFollowingByte))));
            utf8.push_back(FollowingByte(code >> BitsPerFollowingByte));
            utf8.push_back(FollowingByte(code));
        }
    }

    return utf8;
}

// Reads what Utf8FromText writes. A byte that does not fit the pattern is taken as the character with its number.
std::u16string TextFromUtf8(std::string_view utf8)
{
    std::u16string text;
    std::size_t position = 0;
    while (position < utf8.size())
    {
        unsigned int first = static_cast<unsigned char>(utf8[position]);
        bool isFirstOfTwo = (first & FirstOfTwoPatternMask) == FirstOfTwoMark && position + 1 < utf8.size();
        bool isFirstOfThree = (first & FirstOfThreePatternMask) == FirstOfThreeMark && position + 2 < utf8.size();

        if (isFirstOfTwo)
        {
            unsigned int code = (first & FirstOfTwoValueBits) << BitsPerFollowingByte;
            code |= BitsOfFollowingByte(utf8[position + 1]);
            text.push_back(static_cast<char16_t>(code));
            position += 2;
        }
        else if (isFirstOfThree)
        {
            unsigned int code = (first & FirstOfThreeValueBits) << (2 * BitsPerFollowingByte);
            code |= BitsOfFollowingByte(utf8[position + 1]) << BitsPerFollowingByte;
            code |= BitsOfFollowingByte(utf8[position + 2]);
            text.push_back(static_cast<char16_t>(code));
            position += 3;
        }
        else
        {
            text.push_back(static_cast<char16_t>(first));
            position += 1;
        }
    }

    return text;
}

std::string_view Trim(std::string_view text)
{
    std::size_t first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos)
        return {};

    std::size_t last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::optional<int> ReadWholeNumber(std::string_view text)
{
    int number = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (error != std::errc() || end != text.data() + text.size())
        return std::nullopt;

    return number;
}

std::optional<bool> ReadYesOrNo(std::string_view text)
{
    if (text == YesText)
        return true;

    if (text == NoText)
        return false;

    return std::nullopt;
}

std::string_view YesOrNo(bool value)
{
    return value ? YesText : NoText;
}

void ReadWindowSide(std::string_view value, std::uint32_t& side)
{
    std::optional<int> number = ReadWholeNumber(value);
    if (!number)
        return;

    bool isPossibleSide = *number >= static_cast<int>(SmallestWindowSide) && *number <= static_cast<int>(LargestWindowSide);
    if (!isPossibleSide)
        return;

    side = static_cast<std::uint32_t>(*number);
}

std::string KeyLineName(const RemoteButtonName& button)
{
    return std::string(KeyNamePrefix) + Utf8FromText(button.name);
}

// A key that a line gives one button is taken away from any other button that had it until then (by default, or by an earlier
// line of a file changed by hand), so two buttons never share a key.
void GiveKeyToOnlyThisButton(PcSettings& settings, Input::ButtonMask button, int virtualKeyCode)
{
    std::erase_if(settings.keyMap,
        [button, virtualKeyCode](const Input::KeyBinding& binding)
        { return binding.virtualKeyCode == virtualKeyCode && binding.button != button; });
    SetKeyOfButton(settings, button, virtualKeyCode);
}

// "Key A = Enter": the button and the key, both by the names the page shows.
bool ReadKeyLine(std::string_view name, std::string_view value, PcSettings& settings)
{
    for (const RemoteButtonName& button : AssignableButtons())
    {
        if (name != KeyLineName(button))
            continue;

        std::optional<PcKey> key = FindKeyByName(TextFromUtf8(value));
        if (key)
            GiveKeyToOnlyThisButton(settings, button.button, key->virtualKeyCode);

        return true;
    }

    return false;
}

void ReadLine(std::string_view name, std::string_view value, PcSettings& settings)
{
    if (name == WindowWidthName)
    {
        ReadWindowSide(value, settings.windowWidth);
    }
    else if (name == WindowHeightName)
    {
        ReadWindowSide(value, settings.windowHeight);
    }
    else if (name == WindowLeftName)
    {
        settings.windowLeft = ReadWholeNumber(value);
    }
    else if (name == WindowTopName)
    {
        settings.windowTop = ReadWholeNumber(value);
    }
    else if (name == FullscreenName)
    {
        settings.isFullscreen = ReadYesOrNo(value).value_or(settings.isFullscreen);
    }
    else if (name == AudioDeviceName)
    {
        settings.audioDevice = TextFromUtf8(value);
    }
    else if (name == VolumeName)
    {
        std::optional<int> volume = ReadWholeNumber(value);
        if (volume && *volume >= LowestVolume && *volume <= HighestVolume)
            settings.volume = *volume;
    }
    else if (name == MouseButtonsSwappedName)
    {
        settings.areMouseButtonsSwapped = ReadYesOrNo(value).value_or(settings.areMouseButtonsSwapped);
    }
    else if (name == ServerAddressName)
    {
        settings.serverAddress = std::string(value);
    }
    else
    {
        ReadKeyLine(name, value, settings);
    }
}

int PositionOfKey(int virtualKeyCode)
{
    std::span<const PcKey> keys = SelectableKeys();
    for (std::size_t position = 0; position < keys.size(); ++position)
    {
        if (keys[position].virtualKeyCode == virtualKeyCode)
            return static_cast<int>(position);
    }

    return 0;
}

// The first key of the default table for each button: the table has a second key for + and - on the keypad, which the page,
// with one key per button, does not keep.
int DefaultKeyOfButton(Input::ButtonMask button)
{
    for (const Input::KeyBinding& binding : Input::DefaultKeyMap)
    {
        if (binding.button == button)
            return binding.virtualKeyCode;
    }

    return NoKey;
}

}

PcSettings DefaultPcSettings()
{
    PcSettings settings;
    for (const RemoteButtonName& button : AssignableButtons())
        SetKeyOfButton(settings, button.button, DefaultKeyOfButton(button.button));

    return settings;
}

int KeyOfButton(const PcSettings& settings, Input::ButtonMask button)
{
    for (const Input::KeyBinding& binding : settings.keyMap)
    {
        if (binding.button == button)
            return binding.virtualKeyCode;
    }

    return NoKey;
}

void SetKeyOfButton(PcSettings& settings, Input::ButtonMask button, int virtualKeyCode)
{
    std::erase_if(settings.keyMap, [button](const Input::KeyBinding& binding) { return binding.button == button; });
    if (virtualKeyCode == NoKey)
        return;

    settings.keyMap.push_back({virtualKeyCode, button});
    std::sort(settings.keyMap.begin(), settings.keyMap.end(),
        [](const Input::KeyBinding& first, const Input::KeyBinding& second) { return first.button < second.button; });
}

bool IsKeyTakenByAnotherButton(const PcSettings& settings, Input::ButtonMask button, int virtualKeyCode)
{
    if (virtualKeyCode == NoKey)
        return false;

    for (const Input::KeyBinding& binding : settings.keyMap)
    {
        if (binding.virtualKeyCode == virtualKeyCode && binding.button != button)
            return true;
    }

    return false;
}

int NextFreeKey(const PcSettings& settings, Input::ButtonMask button, int direction)
{
    std::span<const PcKey> keys = SelectableKeys();
    int keyCount = static_cast<int>(keys.size());
    int position = PositionOfKey(KeyOfButton(settings, button));

    // "None" is never taken, so the search ends at the latest when it comes round to it.
    while (true)
    {
        position = (position + direction + keyCount) % keyCount;
        int key = keys[position].virtualKeyCode;
        if (!IsKeyTakenByAnotherButton(settings, button, key))
            return key;
    }
}

std::vector<Input::MouseBinding> MouseMapOf(const PcSettings& settings)
{
    if (settings.areMouseButtonsSwapped)
        return {{Platform::MouseButton::Left, Input::RemoteButtons::B}, {Platform::MouseButton::Right, Input::RemoteButtons::A}};

    return {Input::DefaultMouseMap.begin(), Input::DefaultMouseMap.end()};
}

std::string WritePcSettingsText(const PcSettings& settings)
{
    std::string text(FileHeading);
    text += std::format("{} = {}\n", WindowWidthName, settings.windowWidth);
    text += std::format("{} = {}\n", WindowHeightName, settings.windowHeight);
    if (settings.windowLeft && settings.windowTop)
    {
        text += std::format("{} = {}\n", WindowLeftName, *settings.windowLeft);
        text += std::format("{} = {}\n", WindowTopName, *settings.windowTop);
    }
    text += std::format("{} = {}\n", FullscreenName, YesOrNo(settings.isFullscreen));
    text += std::format("{} = {}\n", AudioDeviceName, Utf8FromText(settings.audioDevice));
    text += std::format("{} = {}\n", VolumeName, settings.volume);
    text += std::format("{} = {}\n", MouseButtonsSwappedName, YesOrNo(settings.areMouseButtonsSwapped));
    text += std::format("{} = {}\n", ServerAddressName, settings.serverAddress);

    for (const RemoteButtonName& button : AssignableButtons())
    {
        std::optional<PcKey> key = FindKeyByCode(KeyOfButton(settings, button.button));
        std::u16string_view keyName = key ? key->name : u"None";
        text += std::format("{} = {}\n", KeyLineName(button), Utf8FromText(keyName));
    }

    return text;
}

PcSettings ReadPcSettingsText(std::string_view text)
{
    PcSettings settings = DefaultPcSettings();

    std::istringstream lines{std::string(text)};
    std::string line;
    while (std::getline(lines, line))
    {
        std::string_view trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#')
            continue;

        std::size_t equalsSign = trimmed.find('=');
        if (equalsSign == std::string_view::npos)
            continue;

        ReadLine(Trim(trimmed.substr(0, equalsSign)), Trim(trimmed.substr(equalsSign + 1)), settings);
    }

    return settings;
}

PcSettingsStore::PcSettingsStore(std::filesystem::path settingsFile)
    : file(std::move(settingsFile))
    , values(DefaultPcSettings())
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
        return;

    std::ostringstream text;
    text << stream.rdbuf();
    values = ReadPcSettingsText(text.str());
}

const PcSettings& PcSettingsStore::Values() const
{
    return values;
}

void PcSettingsStore::Store(const PcSettings& newSettings)
{
    values = newSettings;

    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << WritePcSettingsText(values);
    if (!stream)
        throw SettingsError(std::format("The settings of the PC cannot be written to {}", file.string()));
}

}
