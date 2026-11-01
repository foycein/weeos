#pragma once

#include "Channels/ChannelServices.h"
#include "Channels/MiiChannel/MessageWindow.h"
#include "Channels/MiiChannel/MiiChannelData.h"
#include "Channels/MiiChannel/MiiFigures.h"
#include "Channels/MiiChannel/MiiPlaza.h"
#include "Channels/MiiChannel/PlazaGround.h"

#include "SystemMenu/MenuLayout.h"

#include <memory>
#include <optional>
#include <string_view>

namespace Channels
{

// What the plaza asks the channel to do next.
enum class PlazaRequest
{
    // "New Mii": the questions of a new Mii, then the editor.
    MakeNewMii,

    // A Mii was dropped on "Edit Mii".
    EditMii,
};

// The Mii Plaza (squareNigaoe_00 over the 3D plaza, 80041024 and the plaza scene): the buttons on both sides and at the bottom, the Miis
// walking about, the hand that grabs a Mii with A and B and drops it on a button, the camera of + and - and the D-pad.
class PlazaScreen
{
public:
    // The buttons of the plaza, by the panes the program lists at 0x8023B9F0.
    static constexpr std::string_view NewMiiButton = "squButtonNull_05";
    static constexpr std::string_view EditMiiButton = "squButtonNull_06";
    static constexpr std::string_view EraseMiiButton = "squButtonNull_08";
    static constexpr std::string_view HelpButton = "squButtonNull_09";
    static constexpr std::string_view WiiMenuButton = "squButtonNull_03";
    static constexpr std::string_view ParadeButton = "squButtonNull_07";
    static constexpr std::string_view WiiRemoteButton = "squButtonNull_00";
    static constexpr std::string_view WiiFriendButton = "squButtonNull_01";
    static constexpr std::string_view DsButton = "squButtonNull_02";
    static constexpr std::string_view WhistleButton = "squButtonNull_14";

    // Everything given must outlive the screen.
    PlazaScreen(ChannelServices& channelServices, const Layout::LayoutArchive& layouts, const Formats::NumberedMessageFile& messages,
        const MiiFigures& miiFigures, const PlazaGround& plazaGround, const Mii::MiiResources& faceParts, MessageWindow& messageWindow,
        MiiChannelData& channelData, std::uint32_t randomSeed);

    // The Miis of the database come onto the plaza.
    void Start();

    // A Mii that was saved by the editor stands on the plaza now.
    void ShowSavedMii(std::size_t slot);

    std::optional<PlazaRequest> TakeRequest();

    // The Mii that "Edit Mii" was asked for.
    std::optional<std::size_t> MiiToEdit() const;

    const MiiPlaza& Plaza() const;
    MiiPlaza& Plaza();

    // Where a Mii of the plaza is in the picture, for the tests and the name balloon.
    Formats::Vector2 PictureCenterOf(std::size_t slot) const;

    // Without input the Miis go on walking and the answers of the messages are taken, but no button reacts: a message or a dialog is
    // over the plaza, or has just closed with the press of this step.
    // Where the middle of a pane is in the picture, in layout units, for the tests that point at it. (0, 0) for a pane that is hidden.
    Formats::Vector2 PlaceOf(std::string_view paneName) const;

    void Update(bool acceptsInput);
    void Draw();

private:
    enum class Pending
    {
        Nothing,
        Erase,
        ReturnToWiiMenu,
        BeginDs,
        ParadeWelcome,
        ParadeExplained,
    };

    void HandleButtons();
    void HandleHand();
    void HandleCamera();
    void HandleMessageAnswer();
    void DropHeldMii(Formats::Vector2 pointer);
    void ShowNameBalloon(std::size_t slot);
    void UpdateWaits();
    void ShowLoadWindow(std::uint32_t messageNumber);
    void HideLoadWindow();
    bool IsPressed(std::string_view buttonPane) const;
    Graphics::Canvas Canvas() const;

    ChannelServices& services;
    const Formats::NumberedMessageFile& texts;
    const MiiFigures& figures;
    const PlazaGround& ground;
    MessageWindow& message;
    MiiChannelData& data;
    std::unique_ptr<SystemMenu::MenuLayout> layout;
    MiiHeads heads;
    MiiPlaza plaza;

    std::optional<PlazaRequest> request;
    std::optional<std::size_t> miiToEdit;
    std::optional<std::size_t> miiToErase;
    Pending pending = Pending::Nothing;
    bool areSortButtonsShown = false;
    bool isArranged = false;
    bool hasSeenParade = false;
    std::optional<std::size_t> balloonMii;
    int balloonSteps = 0;
    int loadSteps = 0;
    std::uint32_t messageAfterLoad = 0;
};

}
