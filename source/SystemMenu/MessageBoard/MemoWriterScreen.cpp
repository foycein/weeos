#include "SystemMenu/MessageBoard/MemoWriterScreen.h"

#include "SystemMenu/DialogManager.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MessageBoard/BoardInput.h"
#include "SystemMenu/MessageBoard/MessageBoardData.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoftKeyboard.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/WiiMenu/BottomBarScreen.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"

#include "Text/MessageCatalog.h"

#include <algorithm>
#include <cmath>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "board";
constexpr std::string_view PaperLayoutName = "my_Memo_a";

constexpr std::string_view HeaderText = "T_Header";
constexpr std::string_view BodyText = "T_Letter";
constexpr std::string_view HintText = "T_TouchLetter";
constexpr std::string_view MiiText = "T_Nigaoe";
constexpr std::string_view KeyboardFieldText = "T_2l_TextBox";
constexpr std::string_view MiiPicture = "Nigaoe";
constexpr std::string_view TextButton = "B_2l_TextBox";
constexpr std::string_view MiiButton = "B_Nigaoe";

// The texts of the writer (0x8140B5A0) and the message when the console has no Mii (0x8140C368).
constexpr std::uint16_t MemoTitleMessage = 133;
constexpr std::uint16_t AddAMiiMessage = 139;
constexpr std::uint16_t WriteAMemoMessage = 140;
constexpr std::uint16_t NoMiisMessage = 380;
constexpr std::uint16_t OkMessage = 46;

}

MemoWriterScreen::MemoWriterScreen(MenuServices& menuServices)
    : Screen(ScreenId::MemoWriter)
    , services(menuServices)
    , data(*menuServices.messageBoard)
{
}

void MemoWriterScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
}

void MemoWriterScreen::Build()
{
    paper = std::make_unique<LetterPaper>(services.display, *archive, std::string(PaperLayoutName));
    MenuLayout& layout = paper->PaperLayout();
    std::string layoutName(PaperLayoutName);
    openAnimation = layout.BindAnimationToAllPanes(layoutName + "_SelectLetter");
    cancelAnimation = layout.BindAnimationToAllPanes(layoutName + "_ExitLetter");
    postAnimation = layout.BindAnimationToAllPanes(layoutName + "_SendOut");

    layout.SetText(HeaderText, services.messages.Find(MemoTitleMessage));
    layout.SetText(KeyboardFieldText, u"");
    layout.PaneNamed("N_TopBtn").isVisible = false;
    data.chosenMii.reset();
    ShowTheText();
    ShowTheMii();
    layout.StartAnimation(openAnimation);
    layout.Calculate();
}

bool MemoWriterScreen::DrawsWithChildren() const
{
    return true;
}

MenuLayout& MemoWriterScreen::PaperLayout()
{
    return paper->PaperLayout();
}

const std::u16string& MemoWriterScreen::Text() const
{
    return text;
}

const std::optional<MiiData>& MemoWriterScreen::Mii() const
{
    return mii;
}

const std::optional<StoredMessage>& MemoWriterScreen::PostedMemo() const
{
    return postedMemo;
}

BottomBarScreen* MemoWriterScreen::Bar()
{
    return dynamic_cast<BottomBarScreen*>(services.screens.FindScreen(ScreenId::BottomBar));
}

// "Write a memo" shows while the memo has no text (0x81443DF4).
void MemoWriterScreen::ShowTheText()
{
    MenuLayout& layout = paper->PaperLayout();
    layout.SetText(BodyText, text);
    layout.SetText(HintText, text.empty() ? services.messages.Find(WriteAMemoMessage) : std::u16string());

    // The paper of the writer is at least as high as the field of the keyboard engine (T_2l_TextBox).
    float fieldHeight = layout.PaneNamed(KeyboardFieldText).height;
    int fewestLines = static_cast<int>(std::ceil(fieldHeight / paper->LineHeight()));
    paper->MeasureTheText(fewestLines);
}

// The face of the Mii of the memo, and its name where "<-Add a Mii" was.
void MemoWriterScreen::ShowTheMii()
{
    MenuLayout& layout = paper->PaperLayout();
    Formats::Pane& miiPicture = layout.PaneNamed(MiiPicture);
    std::optional<Layout::LoadedTexture> face = mii ? data.portraits.FaceOf(*mii) : std::nullopt;
    miiPicture.isVisible = face.has_value();
    if (face)
        layout.ShowTexture(MiiPicture, *face);

    std::optional<std::u16string> name = mii ? MiiPortraits::NameOf(*mii) : std::nullopt;
    layout.SetText(MiiText, name ? *name : services.messages.Find(AddAMiiMessage));
}

bool MemoWriterScreen::RunMain()
{
    MenuLayout& layout = paper->PaperLayout();
    bool hasEnded = false;
    switch (state)
    {
    case WriterState::Opening:
    {
        bool isBarStill = Bar() == nullptr || !Bar()->IsChanging();
        if (!layout.IsAnimationPlaying(openAnimation) && isBarStill)
            state = WriterState::Writing;
        break;
    }

    case WriterState::Writing:
        RunWriting();
        break;

    case WriterState::Typing:
        if (std::optional<KeyboardAnswer> answer = services.keyboard.TakeAnswer())
        {
            if (answer->wasConfirmed)
                text = answer->text;
            ShowTheText();
            state = WriterState::Writing;
        }
        break;

    case WriterState::ChoosingMii:
        if (!HasChildren() && !services.screens.IsLoadingScreen())
        {
            if (data.chosenMii)
                mii = data.chosenMii;
            ShowTheMii();
            state = WriterState::Writing;
        }
        break;

    case WriterState::TellingThereAreNoMiis:
        if (services.dialogs.TakeAnswer())
            state = WriterState::Writing;
        break;

    case WriterState::Leaving:
        hasEnded = !layout.IsAnimationPlaying(leaveAnimation);
        break;
    }

    layout.Calculate();
    return hasEnded;
}

void MemoWriterScreen::RunWriting()
{
    MenuLayout& layout = paper->PaperLayout();
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
        std::optional<Formats::Vector2> place = PointerPlaceOf(services.input, services.display, playerIndex);
        if (!player || !place || !HasGoneDown(*player, BoardPressButtons))
            continue;

        if (HandleBarPress(*place))
            return;

        for (const Formats::Pane* button : layout.BoundingPanesAt(*place))
        {
            if (button->name == TextButton)
            {
                OpenTheKeyboard(playerIndex);
                return;
            }
            if (button->name == MiiButton)
            {
                OpenTheMiiChooser();
                return;
            }
        }
    }
}

bool MemoWriterScreen::HandleBarPress(Formats::Vector2 place)
{
    BottomBarScreen* bar = Bar();
    std::optional<BarButton> button = bar ? bar->ButtonAt(place) : std::nullopt;
    if (button == BarButton::RightButton)
    {
        Post();
        return true;
    }
    if (button == BarButton::BackButton)
    {
        Cancel();
        return true;
    }
    return false;
}

void MemoWriterScreen::OpenTheKeyboard(int playerIndex)
{
    KeyboardRequest request;
    request.title = services.messages.Find(WriteAMemoMessage);
    request.initialText = text;
    request.rules.maximumLength = LongestMemo;
    request.rules.allowsLineFeeds = true;
    request.playerIndex = playerIndex;
    services.keyboard.Open(request);
    state = WriterState::Typing;
}

// The Mii chooser opens only when the console has a Mii; otherwise the writer says so (0x8140C368).
void MemoWriterScreen::OpenTheMiiChooser()
{
    services.sounds.Request(SoundName::Decide);
    if (MiiPortraits::MiisOfTheConsole(data.nand).empty())
    {
        DialogRequest notice;
        notice.layout = DialogLayout::OneButton;
        notice.message = services.messages.Find(NoMiisMessage);
        notice.buttonAText = services.messages.Find(OkMessage);
        if (services.dialogs.Open(notice))
            state = WriterState::TellingThereAreNoMiis;
        return;
    }

    data.chosenMii.reset();
    services.screens.OpenChild(ScreenId::MiiChooser, ScreenId::MemoWriter, 0);
    state = WriterState::ChoosingMii;
}

// The memo lies at a random place of the day that the board shows, at the time of day of now (0x8140C92C, 0x81342670).
void MemoWriterScreen::Post()
{
    BottomBarScreen* bar = Bar();
    services.sounds.Request(SoundName::Decide);

    LocalTime now = services.wiiMenu.console.GetLocalTime();
    float firstRandomNumber = data.NextRandomNumber(now);
    float secondRandomNumber = data.NextRandomNumber(now);
    Formats::Vector2 place = PlaceForNewLetter(firstRandomNumber, secondRandomNumber);
    Formats::BoardMessage memo = MakeMemo(text, mii, place, now);
    postedMemo = PostMemo(data.store, data.shownDay, memo, now);

    bar->Change(BarChange::MemoWriterCloses);
    bar->Change(BarChange::BackToTheBoard);
    leaveAnimation = postAnimation;
    paper->PaperLayout().StartAnimation(leaveAnimation);
    state = WriterState::Leaving;
}

void MemoWriterScreen::Cancel()
{
    BottomBarScreen* bar = Bar();
    services.sounds.Request(SoundName::Cancel);
    bar->Change(BarChange::MemoWriterCloses);
    bar->Change(BarChange::BackToTheBoard);
    leaveAnimation = cancelAnimation;
    paper->PaperLayout().StartAnimation(leaveAnimation);
    state = WriterState::Leaving;
}

void MemoWriterScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main || !paper)
        return;

    paper->Draw();
}

}
