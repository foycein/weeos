#include "SystemMenu/WiiSettings/SoundPage.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

namespace SystemMenu
{

namespace
{

constexpr int ChoiceCount = 3;

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
    builder.SetTitle(1, u"Sound");

    Settings::SoundMode soundMode = settings.GetSoundMode();
    builder.AddChoice(0, ChoiceCount, u"Mono", soundMode == Settings::SoundMode::Mono, [this] { ChooseSoundMode(Settings::SoundMode::Mono); })
        .sound = PageSound::Surround;
    builder.AddChoice(1, ChoiceCount, u"Stereo", soundMode == Settings::SoundMode::Stereo, [this] { ChooseSoundMode(Settings::SoundMode::Stereo); })
        .sound = PageSound::Surround;
    builder.AddChoice(2, ChoiceCount, u"Surround", soundMode == Settings::SoundMode::Surround,
        [this] { ChooseSoundMode(Settings::SoundMode::Surround); })
        .sound = PageSound::Surround;

    builder.AddBackButton(
        [this]
        {
            settings.SetSoundMode(soundModeAtEntry);
            navigator.GoTo(PageId::FirstIndex);
        });
    builder.AddBottomRight(u"Confirm",
        [this]
        {
            settings.Save();
            navigator.GoTo(PageId::FirstIndex);
        });
}

void SoundPage::ChooseSoundMode(Settings::SoundMode soundMode)
{
    settings.SetSoundMode(soundMode);
    navigator.Refresh();
}

}
