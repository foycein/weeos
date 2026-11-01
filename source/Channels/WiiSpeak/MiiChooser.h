#pragma once

#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakSession.h"

#include "Mii/MiiRecord.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace Channels
{

// The Mii lineup of the scene mii_select (05000_MiiSel, 0x80070A2C of the program): the Miis of the console ten at a time with the
// arrows, and the circles above with the chosen ones. A Mii of the lineup goes into the first free circle, a Mii in a circle leaves
// it. OK ends the choice.
class MiiChooser
{
public:
    // The chat room's Miis take up to six circles, the Mii of a voice message one.
    MiiChooser(WiiSpeakSession& session, std::vector<Mii::MiiRecord> chosenBefore, std::size_t mostChosen);

    // Once in every step. Gives the chosen Miis when OK was pressed.
    std::optional<std::vector<Mii::MiiRecord>> Update();
    void Draw();
    SceneLayout& Layout();

    std::size_t ShownPage() const;
    const std::vector<Mii::MiiRecord>& ChosenMiis() const;

private:
    void PressLineupMii(std::size_t place);
    void PressCircle(std::size_t circle);
    void TurnPage(int step);
    void ShowPage();
    void ShowCircles();
    bool IsChosen(const Mii::MiiRecord& mii) const;
    std::size_t PageCount() const;

    WiiSpeakSession& session;
    SceneLayout layout;
    std::vector<Mii::MiiRecord> chosen;
    std::size_t mostChosen;
    std::size_t page = 0;
};

// Choose Miis of the chat room: the chooser of six, whose choice is saved in the save file.
class ChooseMiisScreen : public WiiSpeakScreen
{
public:
    explicit ChooseMiisScreen(WiiSpeakSession& session);

    std::optional<WiiSpeakScreenName> Update() override;
    void Draw() override;
    std::vector<SceneLayout*> ShownLayouts() override;

    MiiChooser& Chooser();

private:
    WiiSpeakSession& session;
    MiiChooser chooser;
};

}
