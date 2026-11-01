#include "SystemMenu/WiiSettings/PcPages.h"

#include "SystemMenu/WiiSettings/NumberText.h"

#include "Settings/PcKeys.h"

#include <algorithm>
#include <array>

namespace SystemMenu
{

namespace
{

constexpr std::u16string_view Title = u"PC Settings";

struct WindowSizeChoice
{
    std::uint32_t width;
    std::uint32_t height;
};

// The sizes of the window that the page offers: 720p, 900p, 1080p and 1440p, all of the shape 16:9 of the picture.
constexpr auto WindowSizes = std::to_array<WindowSizeChoice>({
    {1280, 720},
    {1600, 900},
    {1920, 1080},
    {2560, 1440},
});

// A list has rows of 432 by 56 points below the tabs, and an arrow up and an arrow down at its right, like the list of countries.
constexpr int ListLeft = 88;
constexpr int ListTop = 76;
constexpr int RowWidth = 432;
constexpr int RowHeight = 56;
constexpr int RowsOnListPage = 5;
constexpr int ListArrowLeft = 528;
constexpr int ListArrowUpTop = 76;
constexpr int ListArrowDownTop = 282;
constexpr int ListArrowSize = 72;

// The volume goes in steps of ten percent, shown in large digits between a minus and a plus button, below the two buttons of the page.
constexpr int VolumeStep = 10;
constexpr PageRectangle VolumeLabelBounds = {24, 222, 560, 44};
constexpr PageRectangle VolumeNumberBounds = {232, 262, 144, 72};
constexpr PageRectangle VolumeLowerBounds = {180, 282, 32, 32};
constexpr PageRectangle VolumeHigherBounds = {396, 282, 32, 32};

constexpr std::u16string_view DefaultDeviceName = u"Default Device of Windows";

std::u16string SizeText(std::uint32_t width, std::uint32_t height)
{
    return NumberText(static_cast<int>(width)) + u" x " + NumberText(static_cast<int>(height));
}

std::u16string OnOffText(bool isOn)
{
    return isOn ? u"On" : u"Off";
}

std::u16string SoundModeText(Settings::SoundMode soundMode)
{
    if (soundMode == Settings::SoundMode::Mono)
        return u"Mono";

    if (soundMode == Settings::SoundMode::Surround)
        return u"Surround";

    return u"Stereo";
}

std::u16string KeyNameOf(int virtualKeyCode)
{
    std::optional<Settings::PcKey> key = Settings::FindKeyByCode(virtualKeyCode);
    if (!key)
        return u"None";

    return std::u16string(key->name);
}

int PositionOfKey(int virtualKeyCode)
{
    std::span<const Settings::PcKey> keys = Settings::SelectableKeys();
    for (std::size_t position = 0; position < keys.size(); ++position)
    {
        if (keys[position].virtualKeyCode == virtualKeyCode)
            return static_cast<int>(position);
    }

    return 0;
}

int ListPageCount(int rowCount)
{
    return std::max(1, (rowCount + RowsOnListPage - 1) / RowsOnListPage);
}

PageRectangle RowBounds(int rowOnPage)
{
    return {ListLeft, ListTop + rowOnPage * RowHeight, RowWidth, RowHeight};
}

}

PcPages::PcPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings, Settings::PcSettingsStore& pcSettingsStore,
    Settings::PcSettingsTarget& pcSettingsTarget)
    : navigator(pageNavigator)
    , settings(systemSettings)
    , pcSettings(pcSettingsStore)
    , pcTarget(pcSettingsTarget)
{
}

bool PcPages::Owns(PageId page) const
{
    return page >= PageId::PcIndex && page <= PageId::PcMouse;
}

void PcPages::EnterPage(PageId page)
{
    if (page == PageId::PcSoundDevice)
    {
        audioDevices = pcTarget.ListAudioDevices();
        shownDevicePage = 0;
    }

    if (page == PageId::PcKeys)
        shownKeyPage = 0;
}

void PcPages::BuildPage(PageId page, Page& content)
{
    PageBuilder builder(content);

    switch (page)
    {
    case PageId::PcIndex:
        BuildPcIndex(builder);
        break;

    case PageId::PcWindow:
        BuildWindow(builder);
        break;

    case PageId::PcWindowSize:
        BuildWindowSize(builder);
        break;

    case PageId::PcSound:
        BuildSound(builder);
        break;

    case PageId::PcSoundDevice:
        BuildSoundDevice(builder);
        break;

    case PageId::PcControls:
        BuildControls(builder);
        break;

    case PageId::PcKeys:
        BuildKeys(builder);
        break;

    default:
        BuildMouse(builder);
        break;
    }
}

void PcPages::BuildPcIndex(PageBuilder& builder)
{
    builder.SetTitle(1, std::u16string(Title));
    builder.AddMenuItem(0, u"Window", [this] { navigator.GoTo(PageId::PcWindow); });
    builder.AddMenuItem(1, u"Sound", [this] { navigator.GoTo(PageId::PcSound); });
    builder.AddMenuItem(2, u"Controls", [this] { navigator.GoTo(PageId::PcControls); });
    builder.AddBackButton([this] { navigator.GoTo(PageId::Index); });
}

void PcPages::BuildWindow(PageBuilder& builder)
{
    const Settings::PcSettings& values = pcSettings.Values();

    builder.SetTitle(2, u"Window");
    builder.AddMenuItem(0, u"Window Size: " + SizeText(values.windowWidth, values.windowHeight), [this] { navigator.GoTo(PageId::PcWindowSize); })
        .name = "Window Size";
    builder.AddMenuItem(1, u"Fullscreen: " + OnOffText(values.isFullscreen),
        [this] { Change([](Settings::PcSettings& changed) { changed.isFullscreen = !changed.isFullscreen; }); })
        .name = "Fullscreen";
    builder.AddBackButton([this] { navigator.GoTo(PageId::PcIndex); });
}

void PcPages::BuildWindowSize(PageBuilder& builder)
{
    const Settings::PcSettings& values = pcSettings.Values();
    builder.SetTitle(3, u"Window Size");

    for (std::size_t sizeNumber = 0; sizeNumber < WindowSizes.size(); ++sizeNumber)
    {
        WindowSizeChoice size = WindowSizes[sizeNumber];
        std::u16string text = SizeText(size.width, size.height);

        PageButton& row = builder.AddPictureButton(ButtonLook::CountryItem, RowBounds(static_cast<int>(sizeNumber)), NameOfText(text),
            [this, size]
            {
                Change(
                    [size](Settings::PcSettings& changed)
                    {
                        changed.windowWidth = size.width;
                        changed.windowHeight = size.height;
                    });
            });
        row.text = text;
        row.isChosen = values.windowWidth == size.width && values.windowHeight == size.height;
    }

    builder.AddBackButton([this] { navigator.GoTo(PageId::PcWindow); });
}

void PcPages::BuildSound(PageBuilder& builder)
{
    const Settings::PcSettings& values = pcSettings.Values();
    std::u16string deviceName = values.audioDevice.empty() ? std::u16string(DefaultDeviceName) : values.audioDevice;

    builder.SetTitle(2, u"Sound");
    builder.AddMenuItem(0, deviceName, [this] { navigator.GoTo(PageId::PcSoundDevice); }).name = "Device";
    std::u16string soundModeText = u"Sound Mode: " + SoundModeText(settings.GetSoundMode());
    builder.AddMenuItem(1, soundModeText, [this] { navigator.GoTo(PageId::SoundSet); }).name = "Sound Mode";

    builder.AddText(VolumeLabelBounds, TextRole::Message, {u"Volume"});
    builder.AddText(VolumeNumberBounds, TextRole::BigNumber, {NumberText(values.volume)});

    PageButton& lower = builder.AddPictureButton(ButtonLook::MinusCircle, VolumeLowerBounds, "Quieter", [this] { ChangeVolume(-VolumeStep); });
    lower.isShown = values.volume > Settings::LowestVolume;
    lower.repeatsWhenHeld = true;
    PageButton& higher = builder.AddPictureButton(ButtonLook::PlusCircle, VolumeHigherBounds, "Louder", [this] { ChangeVolume(VolumeStep); });
    higher.isShown = values.volume < Settings::HighestVolume;
    higher.repeatsWhenHeld = true;

    builder.AddBackButton([this] { navigator.GoTo(PageId::PcIndex); });
}

// The default device of Windows first, then every playback device of the PC by its name.
void PcPages::BuildSoundDevice(PageBuilder& builder)
{
    builder.SetTitle(3, u"Device");

    std::vector<std::u16string> rows = {std::u16string(DefaultDeviceName)};
    rows.insert(rows.end(), audioDevices.begin(), audioDevices.end());

    int firstRow = shownDevicePage * RowsOnListPage;
    int lastRow = std::min(firstRow + RowsOnListPage, static_cast<int>(rows.size()));
    for (int rowNumber = firstRow; rowNumber < lastRow; ++rowNumber)
    {
        std::u16string device = rowNumber == 0 ? u"" : rows[rowNumber];

        PageButton& row = builder.AddPictureButton(ButtonLook::CountryItem, RowBounds(rowNumber - firstRow), NameOfText(rows[rowNumber]),
            [this, device] { Change([device](Settings::PcSettings& changed) { changed.audioDevice = device; }); });
        row.text = rows[rowNumber];
        row.isChosen = pcSettings.Values().audioDevice == device;
    }

    AddListArrows(builder, shownDevicePage, ListPageCount(static_cast<int>(rows.size())));
    builder.AddBackButton([this] { navigator.GoTo(PageId::PcSound); });
}

void PcPages::BuildControls(PageBuilder& builder)
{
    builder.SetTitle(2, u"Controls");
    builder.AddMenuItem(0, u"Keys", [this] { navigator.GoTo(PageId::PcKeys); });
    builder.AddMenuItem(1, u"Mouse", [this] { navigator.GoTo(PageId::PcMouse); });
    builder.AddMenuItem(2, u"Rumble: " + OnOffText(settings.IsRumbleOn()), [this] { ToggleRumble(); }).name = "Rumble";
    builder.AddBackButton([this] { navigator.GoTo(PageId::PcIndex); });
}

// One row for each button of the remote with the key that presses it. Pressing a row gives the button the next key of the list.
void PcPages::BuildKeys(PageBuilder& builder)
{
    builder.SetTitle(3, u"Keys");

    std::span<const Settings::RemoteButtonName> buttons = Settings::AssignableButtons();
    int firstRow = shownKeyPage * RowsOnListPage;
    int lastRow = std::min(firstRow + RowsOnListPage, static_cast<int>(buttons.size()));
    for (int rowNumber = firstRow; rowNumber < lastRow; ++rowNumber)
    {
        const Settings::RemoteButtonName& button = buttons[rowNumber];
        int key = Settings::KeyOfButton(pcSettings.Values(), button.button);

        PageButton& row = builder.AddPictureButton(ButtonLook::CountryItem, RowBounds(rowNumber - firstRow), "Key " + NameOfText(button.name),
            [this, mask = button.button] { ChangeKey(mask, 1); });
        row.text = std::u16string(button.name) + u":  " + KeyNameOf(key);
        row.repeatsWhenHeld = true;
    }

    AddListArrows(builder, shownKeyPage, ListPageCount(static_cast<int>(buttons.size())));
    builder.AddBackButton([this] { navigator.GoTo(PageId::PcControls); });
}

void PcPages::BuildMouse(PageBuilder& builder)
{
    bool isSwapped = pcSettings.Values().areMouseButtonsSwapped;

    builder.SetTitle(3, u"Mouse");
    builder.AddChoice(0, 2, u"Left button A, right button B", !isSwapped,
        [this] { Change([](Settings::PcSettings& changed) { changed.areMouseButtonsSwapped = false; }); })
        .name = "Left Is A";
    builder.AddChoice(1, 2, u"Left button B, right button A", isSwapped,
        [this] { Change([](Settings::PcSettings& changed) { changed.areMouseButtonsSwapped = true; }); })
        .name = "Left Is B";
    builder.AddBackButton([this] { navigator.GoTo(PageId::PcControls); });
}

void PcPages::AddListArrows(PageBuilder& builder, int& shownListPage, int listPageCount)
{
    PageButton& up = builder.AddPictureButton(ButtonLook::ArrowUp, {ListArrowLeft, ListArrowUpTop, ListArrowSize, ListArrowSize}, "PreviousRows",
        [this, &shownListPage]
        {
            --shownListPage;
            navigator.Refresh();
        });
    up.isShown = shownListPage > 0;
    up.sound = PageSound::Scroll;

    PageButton& down = builder.AddPictureButton(ButtonLook::ArrowDown, {ListArrowLeft, ListArrowDownTop, ListArrowSize, ListArrowSize}, "NextRows",
        [this, &shownListPage]
        {
            ++shownListPage;
            navigator.Refresh();
        });
    down.isShown = shownListPage < listPageCount - 1;
    down.sound = PageSound::Scroll;
}

void PcPages::Change(const std::function<void(Settings::PcSettings&)>& change)
{
    Settings::PcSettings changed = pcSettings.Values();
    change(changed);
    pcSettings.Store(changed);
    pcTarget.ApplyPcSettings(changed);
    navigator.Refresh();
}

void PcPages::ToggleRumble()
{
    bool isRumbleOn = !settings.IsRumbleOn();
    settings.SetRumble(isRumbleOn);
    settings.Save();
    pcTarget.ApplyRumble(isRumbleOn);
    navigator.Refresh();
}

void PcPages::ChangeVolume(int change)
{
    Change([change](Settings::PcSettings& changed)
        { changed.volume = std::clamp(changed.volume + change, Settings::LowestVolume, Settings::HighestVolume); });
}

// The list of keys goes round: after the last key comes "None" again.
void PcPages::ChangeKey(Input::ButtonMask button, int change)
{
    Change(
        [button, change](Settings::PcSettings& changed)
        {
            int keyCount = static_cast<int>(Settings::SelectableKeys().size());
            int position = PositionOfKey(Settings::KeyOfButton(changed, button)) + change;
            int positionInTheList = (position + keyCount) % keyCount;
            Settings::SetKeyOfButton(changed, button, Settings::SelectableKeys()[positionInTheList].virtualKeyCode);
        });
}

}
