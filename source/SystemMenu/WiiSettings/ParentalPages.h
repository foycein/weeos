#pragma once

#include "SystemMenu/WiiSettings/PageBuilder.h"
#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Ios/Network/NetworkConfigService.h"
#include "Settings/ConsoleClock.h"
#include "Settings/SystemSettings.h"

namespace SystemMenu
{

// "Parental Controls", as the pages of the original lead the user (Parental_Control in html/US2/iplsetting.ash and its script
// FIX/js/US/COM/Parental_Control.js): the setup asks for a PIN twice, a secret question and its answer, and then offers the game
// rating and the other restrictions; nothing is written before "Settings Complete". Controls that are on ask for the PIN first, or
// for the answer, or for the master key that turns them off.
//
// The other restrictions are the Internet Channel, the Wii Shop Channel and the News Channel. The restriction of the messages of
// WiiConnect24 is not offered, because WeeOS has no WiiConnect24 (docs/DECISIONS.md).
class ParentalPages : public PageGroup
{
public:
    // The navigator, the settings, the clock and the network configuration (for the address of the wireless card) must outlive the pages.
    ParentalPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings, const Settings::ConsoleClock& consoleClock,
        Ios::NetworkConfigService& networkConfigService);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;

private:
    void BuildSetupPage(PageId page, PageBuilder& builder);
    void BuildSettingsPage(PageId page, PageBuilder& builder);
    void BuildCheckPage(PageId page, PageBuilder& builder);

    void BuildPinChoice(PageBuilder& builder);
    void BuildPinRepeat(PageBuilder& builder);
    void BuildQuestionChoice(PageBuilder& builder);
    void BuildAnswerChoice(PageBuilder& builder);
    void BuildSettingsMenu(PageBuilder& builder);
    void BuildSettingsOverview(PageBuilder& builder);
    void BuildRatingChoice(PageBuilder& builder);
    void BuildRestriction(PageBuilder& builder);
    void BuildPinCheck(PageBuilder& builder);
    void BuildAnswerCheck(PageBuilder& builder);
    void BuildMasterKeyInformation(PageBuilder& builder);
    void BuildMasterKeyEntry(PageBuilder& builder);

    // A page with a message and one button "OK" that leads to the next page.
    void BuildNotice(PageBuilder& builder, std::vector<std::u16string> lines, PageId nextPage);

    // The field that opens the keyboard. A PIN shows a star for each digit.
    void AddField(PageBuilder& builder, int slot, int slotCount, std::u16string& typedText, InputKind kind, std::size_t maximumLength);

    void AcceptRepeatedPin();
    void AcceptAnswer();
    void CheckPin();
    void CheckAnswer();
    void CheckMasterKey();
    void ChangeRating(int change);
    void ChooseRestriction(bool isRestricted);
    void ShowNextRestriction();
    void CompleteSettings();
    void ClearSettings();

    // Where the pages of the PIN, the question and the answer go back to: the next step of the setup, or the overview of the settings.
    PageId PageAfterTheSecret(PageId nextStepOfTheSetup) const;

    PageNavigator& navigator;
    Settings::SystemSettings& settings;
    const Settings::ConsoleClock& clock;
    Ios::NetworkConfigService& networkConfig;

    // The parental controls and the restrictions as the user makes them. They are written when the user completes the settings.
    Settings::ParentalControls draft;
    Settings::ParentalRestrictions draftRestrictions;

    // True while the user changes one secret from the overview, false during the first setup.
    bool isChangingOneSetting = false;

    std::u16string typedPin;
    std::u16string typedRepeat;
    std::u16string typedAnswer;
    std::u16string typedCheck;

    // The rating and the restriction that the pages show, counted from 0.
    int shownRating = 0;
    int shownRestriction = 0;
};

}
