#pragma once

#include "Channels/ChannelServices.h"
#include "Channels/MiiChannel/MessageWindow.h"
#include "Channels/MiiChannel/MiiChannelData.h"
#include "Channels/MiiChannel/MiiEditor.h"
#include "Channels/MiiChannel/MiiFigures.h"
#include "Channels/MiiChannel/ProfileDialog.h"

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/SoftKeyboard.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Channels
{

struct EditorResult
{
    bool wasSaved = false;

    // The slot the Mii was saved into.
    std::optional<std::size_t> slot;
};

// The editor of the Mii Channel (editNigaoe_00, the page functions 8003CBDC to 8003F26C): the tabs at the top, the page of the tab with its
// grids of parts, palettes and the buttons that size and move a part, the Mii itself standing on the left, and "Quit" with its three
// choices. Saving writes the Mii into RFL_DB.dat at once.
class EditorScreen
{
public:
    // Everything given must outlive the screen.
    EditorScreen(ChannelServices& channelServices, const Layout::LayoutArchive& layouts, const Formats::NumberedMessageFile& messages,
        const MiiFigures& miiFigures, const Mii::MiiResources& faceParts, MessageWindow& messageWindow, ProfileDialog& profileDialog,
        SystemMenu::SoftKeyboard& softKeyboard, MiiChannelData& channelData);

    // A Mii of the plaza keeps its slot; a new Mii gets one when it is saved.
    void Open(const Mii::MiiRecord& record, std::optional<std::size_t> slot);

    bool IsOpen() const;
    std::optional<EditorResult> TakeResult();

    const MiiEditor& Editor() const;
    EditorPage Page() const;

    // The panes of the screen, for the tests: a tab, a place of a grid, a color, an adjusting button.
    static std::string TabButton(EditorPage page);
    std::string GridButton(EditorPart part, int place) const;
    std::string ColorButton(ColorPart part, int color) const;
    std::string AdjustButton(EditorPart part, Adjustment adjustment) const;
    static constexpr std::string_view QuitButton = "windowOK_00";
    static constexpr std::string_view NicknameButton = "frmProfile_00";

    // Where the middle of a pane is in the picture, in layout units, for the tests that point at it. (0, 0) for a pane that is hidden.
    Formats::Vector2 PlaceOf(std::string_view paneName) const;

    // Without input the Mii moves and the answers of the dialogs are taken, but no button reacts.
    void Update(bool acceptsInput);
    void Draw();

private:
    enum class Pending
    {
        Nothing,
        QuitChoice,
        QuitWithoutSaving,
        CompleteProfile,
        PlazaIsFull,
    };

    void ShowPage(EditorPage page);
    void ShowSubPart(int subPart);
    std::vector<EditorPart> PartsOfPage(EditorPage page) const;
    EditorPart ShownPart() const;
    void RefreshSelections();
    void RefreshGridPage(EditorPart part);
    void RefreshProfile();
    void RefreshBodySliders();

    bool HandlePageButtons();
    bool HandleGrid(EditorPart part);
    bool HandleGridArrows(EditorPart part);
    bool HandlePalette(ColorPart part);
    bool HandleAdjustButtons(EditorPart part);
    bool HandleSubTabs();
    bool HandleBodySliders();
    bool HandleProfileRows();
    void Ask(ProfileQuestion question);
    void HandleDialogAnswers();
    void HandleKeyboardAnswer();
    void HandleMessageAnswer();
    void AskQuit();
    void Save();

    bool IsPressed(std::string_view buttonPane) const;
    PictureRectangle RectangleOf(std::string_view paneName) const;
    std::optional<ColorPart> ColorPartOf(EditorPart part) const;

    ChannelServices& services;
    const Formats::NumberedMessageFile& texts;
    const MiiFigures& figures;
    MessageWindow& message;
    ProfileDialog& profile;
    SystemMenu::SoftKeyboard& keyboard;
    MiiChannelData& data;
    std::unique_ptr<SystemMenu::MenuLayout> layout;
    MiiHeads heads;

    std::optional<MiiEditor> editor;
    std::optional<std::size_t> editedSlot;
    std::optional<EditorResult> result;
    EditorPage page = EditorPage::Profile;
    int subPart = 0;
    std::array<int, 16> gridPages = {};
    Pending pending = Pending::Nothing;
    bool isTypingCreator = false;
    std::optional<ProfileQuestion> askedQuestion;
    std::vector<std::u16string> creatorChoices;
    int chosenBirthMonth = 0;
    std::optional<int> draggedSlider;
    float waitFrame = 0;
};

}
