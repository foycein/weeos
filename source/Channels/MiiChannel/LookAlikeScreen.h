#pragma once

#include "Channels/ChannelServices.h"
#include "Channels/MiiChannel/MessageWindow.h"
#include "Channels/MiiChannel/MiiFigures.h"

#include "SystemMenu/MenuLayout.h"

#include "Mii/MiiRecord.h"
#include "Mii/RandomMii.h"

#include <memory>
#include <optional>
#include <random>
#include <vector>

namespace Channels
{

struct LookAlikeResult
{
    bool wasCancelled = false;

    // The face that was chosen, with the parts of a random Mii: no names, no create id.
    Mii::MiiRecord record;
};

// "Choose a look-alike" (selectNigaoe_00, 80060518 and 800601E8): first many random faces of the gender; the face that is picked then
// stands in the middle of nine, and the eight around it are made again as adults, East Asian in the top row, light in the middle and dark
// in the bottom one. Every pick moves the picked face to the middle. "Use this face" asks once more and gives the face to the editor.
class LookAlikeScreen
{
public:
    static constexpr int FirstFaceCount = 43;
    static constexpr int GridFaceCount = 9;
    static constexpr int MiddleOfGrid = 4;

    LookAlikeScreen(ChannelServices& channelServices, const Layout::LayoutArchive& layouts, const Formats::NumberedMessageFile& messages,
        const MiiFigures& miiFigures, const Mii::MiiResources& faceParts, MessageWindow& messageWindow, std::uint32_t randomSeed);

    void Start(Mii::Gender newGender);
    bool IsOpen() const;
    std::optional<LookAlikeResult> TakeResult();

    // The faces of the grid, for the tests: the first ones, or the nine with the pick in the middle.
    const std::vector<Mii::MiiRecord>& Faces() const;
    bool IsRefining() const;

    // The pane that a face is drawn in and picked with.
    std::string CardPane(int faceNumber) const;
    static constexpr std::string_view UseThisFaceButton = "frmOK_00";

    // Where the middle of a pane is in the picture, in layout units, for the tests that point at it. (0, 0) for a pane that is hidden.
    Formats::Vector2 PlaceOf(std::string_view paneName) const;

    // Without input the faces are made and the answer of the question is taken, but no card reacts.
    void Update(bool acceptsInput);
    void Draw();

private:
    void HandleConfirmation();
    void HandleButtons();
    void MakeFirstFaces();
    void Pick(int faceNumber);
    void ShowStage();
    Mii::MiiAge RandomAge();
    bool IsPressed(std::string_view buttonPane) const;
    void Finish(LookAlikeResult finished);

    ChannelServices& services;
    const MiiFigures& figures;
    MessageWindow& message;
    std::unique_ptr<SystemMenu::MenuLayout> layout;
    MiiHeads heads;
    std::mt19937 random;

    bool isOpen = false;
    bool isRefining = false;
    bool isConfirming = false;
    Mii::Gender gender = Mii::Gender::Male;
    std::vector<Mii::MiiRecord> faces;
    std::optional<LookAlikeResult> result;
};

}
