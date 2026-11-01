#include "SystemMenu/WiiSettings/SoundPage.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

#include <array>
#include <string_view>

namespace SystemMenu
{

namespace
{

struct SoundModeChoice
{
    std::u16string_view text;
    Settings::SoundMode soundMode;
};

constexpr int ChoiceCount = 3;

constexpr std::array<SoundModeChoice, ChoiceCount> SoundModeChoices = {{
    {u"Mono", Settings::SoundMode::Mono},
    {u"Stereo", Settings::SoundMode::Stereo},
    {u"Surround", Settings::SoundMode::Surround},
}};

}

SoundPage::SoundPage(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings)
    : navigator(pageNavigator)
    , settings(systemSettings)
{
}

bool SoundPage::Owns(PageId page) const
{
    return page == PageId::SoundSet;
}

void SoundPage::EnterPage(PageId page)
{
    static_cast<void>(page);
    soundModeAtEntry = settings.GetSoundMode();
}

void SoundPage::BuildPage(PageId page, Page& content)
{
    static_cast<void>(page);

    PageBuilder builder(content);
    builder.SetTitle(3, u"Sound Mode");

    Settings::SoundMode chosenMode = settings.GetSoundMode();
    for (int slot = 0; slot < ChoiceCount; ++slot)
    {
        Settings::SoundMode soundMode = SoundModeChoices[slot].soundMode;
        bool isChosen = soundMode == chosenMode;
        std::u16string text(SoundModeChoices[slot].text);
        PageButton& button = builder.AddChoice(slot, ChoiceCount, text, isChosen, [this, soundMode] { ChooseSoundMode(soundMode); });
        button.sound = PageSound::Surround;
    }

    builder.AddBackButton(
        [this]
        {
            settings.SetSoundMode(soundModeAtEntry);
            navigator.GoTo(PageId::PcSound);
        });
    builder.AddBottomRight(u"Confirm",
        [this]
        {
            settings.Save();
            navigator.GoTo(PageId::PcSound);
        });
}

void SoundPage::ChooseSoundMode(Settings::SoundMode soundMode)
{
    settings.SetSoundMode(soundMode);
    navigator.Refresh();
}

}
