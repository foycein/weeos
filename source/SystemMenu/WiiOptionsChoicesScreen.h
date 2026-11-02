#pragma once

#include "SystemMenu/ButtonController.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/WiiOptionsBarScreen.h"

#include "Layout/LayoutArchive.h"

#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The buttons of Wii Options (screen 0x13). They come in pages. A button leads to the next page, to another screen or to an editor,
// and the Back button of the bar leads one page back, and from the first page out of Wii Options.
//
//     Options page:          Data Management, Wii Settings (leaves Wii Options for the screen of Wii Settings)
//     Data Management page:  Save Data, Channels (opens the editor of the channels)
//     Save Data page:        Wii (opens the editor of the save data of the Wii)
//
// The button of the GameCube on the Save Data page is not part of WeeOS.
//
// The argument is 0 when the screen starts with the Options page. Any other number starts it with the Data Management page.
class WiiOptionsChoicesScreen : public Screen
{
public:
    WiiOptionsChoicesScreen(MenuServices& menuServices, int screenArgument);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    // The screen goes on and is drawn while an editor is open on top of it.
    bool UpdatesWithChildren() const override;
    bool DrawsWithChildren() const override;

protected:
    bool RunMain() override;

private:
    enum class Page
    {
        Options,
        DataManagement,
        SaveData,
    };

    enum class Choice
    {
        DataManagement,
        WiiSettings,
        SaveData,
        Channels,
        Wii,
    };

    static constexpr std::size_t ChoiceCount = 5;

    // What the screen is doing. The numbers are those of the states of the original (0x81408294).
    enum class State
    {
        WaitingForTheBar = 0,
        OptionsAppearing = 1,
        OptionsReady = 2,
        OptionsLeaving = 3,
        DataManagementAppearing = 4,
        DataManagementReady = 5,
        DataManagementLeaving = 6,
        SaveDataAppearing = 7,
        SaveDataReady = 8,
        SaveDataLeaving = 9,
        ReturningToOptions = 10,
        ReturningToDataManagement = 11,
        BackButtonPressed = 12,
        FadingOut = 13,
        WaitingForEditor = 14,
        EditorEnded = 15,
    };

    // The names that bind the layout to a button.
    struct ChoiceNames
    {
        Page page;
        std::string_view buttonPane;
        std::string_view buttonGroup;
        std::string_view shadeGroup;

        // The files of the animations of the page: "it_ObjSetUp_a_<stem>In", "<stem>FoucusIn", and so on.
        std::string_view animationStem;
    };

    // The animations of one button. An animation file moves the panes of every button of its page, so each button binds the file to its
    // own groups: the group of the button, and the group of the shade that lies on the button while a pointer is on it.
    struct ButtonAnimations
    {
        std::size_t appear = 0;
        std::size_t focusIn = 0;
        std::size_t focusOut = 0;
        std::size_t flashOfButton = 0;
        std::size_t flashOfShade = 0;
        std::size_t disappear = 0;
        std::size_t comeBack = 0;
    };

    static const ChoiceNames& NamesOf(Choice choice);
    static std::optional<Choice> ChoiceOfPane(std::string_view buttonPane);
    static std::optional<Choice> OtherChoiceOfPage(Choice choice);
    const ButtonAnimations& AnimationsOf(Choice choice) const;

    void BindAnimationsOfButton(Choice choice);
    std::size_t BindAnimation(const ChoiceNames& names, std::string_view ending, std::string_view group);
    void SetTextsOfButtons();
    void EnableButtonsOfPage(Page page);

    void MoveToState(State newState);
    void StartAnimation(std::size_t slot);
    bool AreAnimationsFinished() const;
    void WaitForAnimations(State nextState);
    WiiOptionsBarScreen* FindBar() const;

    void StartFirstPage();
    void RunPageReady();
    void HandleButtonEvents(const std::vector<ButtonEvent>& events);
    static State ReadyStateOf(Page page);
    bool IsButtonOnCurrentPage(Choice choice) const;
    void PressButton(Choice choice);
    void OpenEditor(ScreenId editor);
    void FinishLeavingOptionsPage();
    void FinishLeavingDataManagementPage();
    void FinishLeavingSaveDataPage();
    void ShowPage(Page page);
    void ReactToBackButton();
    void LeaveWiiOptions();
    void ReturnToPage(Page page);
    void WaitForEditorToClose();
    void FinishReturningFromEditor();

    MenuServices& services;
    int argument;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    std::unique_ptr<ButtonController> buttons;
    std::array<ButtonAnimations, ChoiceCount> animations;
    std::vector<std::size_t> allSlots;

    State state = State::WaitingForTheBar;

    // The state in which the user pressed the Back button, and the one that came before the state of the moment.
    State previousState = State::WaitingForTheBar;

    // The button that was pressed last, and where it leads when it leads somewhere else than to the next page.
    std::optional<Choice> pressedChoice;
    std::optional<ScreenId> destination;
};

}
