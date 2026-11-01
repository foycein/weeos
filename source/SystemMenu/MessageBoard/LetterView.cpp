#include "SystemMenu/MessageBoard/LetterView.h"

#include "SystemMenu/DialogManager.h"
#include "SystemMenu/MessageBoard/BoardInput.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/WiiMenu/BottomBarScreen.h"
#include "SystemMenu/WiiMenu/LayoutParts.h"

#include "Text/MessageCatalog.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace SystemMenu
{

namespace
{

constexpr std::string_view HeaderText = "T_Header";
constexpr std::string_view BodyText = "T_Letter";
constexpr std::string_view MiiPicture = "Nigaoe";
constexpr std::array<std::string_view, 3> TextsOfTheWriter = {"T_TouchLetter", "T_Nigaoe", "T_2l_TextBox"};
// The arrow of the group R stands at the top of the paper and points up; the arrow of the group L stands at the bottom.
constexpr std::string_view UpArrowButton = "B_ArwR";
constexpr std::string_view DownArrowButton = "B_ArwL";

// The messages of the view (0x81398EF4, 0x8138FC4C, 0x81397768).
constexpr std::uint16_t MemoTitleMessage = 133;
constexpr std::uint16_t BackMessage = 315;
constexpr std::uint16_t TrashMessage = 40;
constexpr std::uint16_t EraseQuestionMessage = 64;
constexpr std::uint16_t OkMessage = 46;
constexpr std::uint16_t QuitMessage = 37;

constexpr std::string_view UnselectSound = "WIPL_SE_BOARD_UNSELECT";
constexpr std::string_view ThrowSound = "WIPL_SE_BOARD_DUMP";
constexpr std::string_view ScrollSound = "WIPL_SE_MESSAGE_SCROLL";
constexpr std::string_view ZoomInSound = "WIPL_SE_PIC_ZOOM_IN";
constexpr std::string_view ZoomOutSound = "WIPL_SE_PIC_ZOOM_OUT";

// The picture of a letter: its frame, the pane that shows it, the button that zooms it and the group of its animations (0x81399E3C).
constexpr std::string_view PictureFrame = "N_Pic";
constexpr std::string_view PicturePane = "Pic";
constexpr std::string_view PictureButton = "B_Pic";
constexpr std::string_view PictureGroup = "G_Pic";

// The paper slides from the small letter to the middle of the picture while it opens (0x81397374: 17 steps).
constexpr int StepsOfTheOpening = 17;

// An arrow of the paper moves it 300 units in 20 steps (0x81363CF8).
constexpr float ScrollOfAnArrow = 300;
constexpr int StepsOfAnArrowScroll = 20;

// The D-pad speeds the paper up: speed = speed * 0.6 + 1.5 in every step (0x81363B80); faster than 1 unit a step it makes a sound.
constexpr float DPadKeptSpeed = 0.6f;
constexpr float DPadPush = 1.5f;
constexpr float SpeedThatSounds = 1;

std::string LayoutNameOf(Formats::BoardMessageKind kind)
{
    bool isOnMemoPaper = kind == Formats::BoardMessageKind::Memo || kind == Formats::BoardMessageKind::PlayLog;
    return isOnMemoPaper ? "my_Memo_a" : "my_LetterL";
}

float Smoothed(float progress)
{
    return progress * progress * (3 - 2 * progress);
}

}

LetterView::LetterView(MenuServices& menuServices, const Layout::LayoutArchive& boardArchive, const StoredMessage& message,
    MiiPortraits& portraits, BottomBarScreen& bottomBar, Formats::Vector2 placeOfTheSmallLetter)
    : services(menuServices)
    , bar(bottomBar)
    , paper(menuServices.display, boardArchive, LayoutNameOf(message.message.kind))
    , layout(paper.PaperLayout())
    , openingPlace(placeOfTheSmallLetter)
{
    std::string layoutName = LayoutNameOf(message.message.kind);
    openAnimation = layout.BindAnimationToAllPanes(layoutName + "_SelectLetter");
    closeAnimation = layout.BindAnimationToAllPanes(layoutName + "_ExitLetter");
    arrowLoopAnimation = layout.BindAnimation(layoutName + "_Loop", "G_ArwRoop", Layout::PlaybackMode::Loop);
    upArrowAppears = layout.BindAnimation(layoutName + "_Appear", "G_ArwR_End", Layout::PlaybackMode::PlayOnce);
    upArrowGoes = layout.BindAnimation(layoutName + "_Lost", "G_ArwR_End", Layout::PlaybackMode::PlayOnce);
    downArrowAppears = layout.BindAnimation(layoutName + "_Appear", "G_ArwL_End", Layout::PlaybackMode::PlayOnce);
    downArrowGoes = layout.BindAnimation(layoutName + "_Lost", "G_ArwL_End", Layout::PlaybackMode::PlayOnce);

    FillThePaper(message, portraits);
    ShowThePicture(message);
    paper.MeasureTheText();
    layout.StartAnimation(openAnimation);
    layout.StartAnimation(arrowLoopAnimation);
    PutTheRootAtTheOpeningPlace();
    layout.Calculate();

    bool isPlayLog = message.message.kind == Formats::BoardMessageKind::PlayLog;
    bar.SetMode(isPlayLog ? BarMode::PlayLog : BarMode::Letter);
    bar.SetBackText(services.messages.Find(BackMessage));
    bar.SetTrashText(services.messages.Find(TrashMessage));
    bar.Change(BarChange::LetterOpens);
    bar.Change(isPlayLog ? BarChange::PlayLogShows : BarChange::LetterShowsWithoutButton);
}

void LetterView::FillThePaper(const StoredMessage& message, MiiPortraits& portraits)
{
    bool isMemo = message.message.kind == Formats::BoardMessageKind::Memo;
    layout.SetText(HeaderText, isMemo ? services.messages.Find(MemoTitleMessage) : message.message.subject);
    layout.SetText(BodyText, message.message.body);
    for (std::string_view writerText : TextsOfTheWriter)
    {
        if (layout.FindPane(writerText) != nullptr)
            layout.SetText(writerText, u"");
    }

    Formats::Pane& miiPicture = layout.PaneNamed(MiiPicture);
    miiPicture.isVisible = false;
    if (!message.message.mii)
        return;

    std::optional<Layout::LoadedTexture> face = portraits.FaceOf(*message.message.mii);
    if (!face)
        return;
    layout.ShowTexture(MiiPicture, *face);
    miiPicture.isVisible = true;
}

// The picture fills the pane Pic and keeps its shape (0x81399FEC). Pressing it zooms it (SelectPic), Back puts it back (ExitPic).
void LetterView::ShowThePicture(const StoredMessage& message)
{
    Formats::Pane* frame = layout.FindPane(PictureFrame);
    if (frame == nullptr)
        return;

    picture = LetterPicture::Of(services.display.Device(), message.message);
    frame->isVisible = picture != nullptr;
    if (Formats::Pane* button = layout.FindPane(PictureButton))
        button->isVisible = picture != nullptr;
    if (!picture)
        return;

    Formats::Pane& pane = layout.PaneNamed(PicturePane);
    float paneAspect = pane.width / pane.height;
    if (picture->Aspect() > paneAspect)
        pane.height = pane.width / picture->Aspect();
    else
        pane.width = pane.height * picture->Aspect();
    layout.ShowTexture(PicturePane, picture->Texture());

    std::string layoutName = "my_LetterL";
    zoomInAnimation = layout.BindAnimation(layoutName + "_SelectPic", PictureGroup, Layout::PlaybackMode::PlayOnce);
    zoomOutAnimation = layout.BindAnimation(layoutName + "_ExitPic", PictureGroup, Layout::PlaybackMode::PlayOnce);
}

void LetterView::PutTheRootAtTheOpeningPlace()
{
    float progress = std::min(1.0f, static_cast<float>(stepsSinceOpening) / StepsOfTheOpening);
    float remaining = 1 - Smoothed(progress);
    MoveLayoutTo(layout, Formats::Vector2{openingPlace.x * remaining, openingPlace.y * remaining});
}

LetterPaper& LetterView::Paper()
{
    return paper;
}

bool LetterView::ShowsAPicture() const
{
    return picture != nullptr;
}

bool LetterView::IsPictureZoomed() const
{
    return state == ViewState::ViewingThePicture;
}

float LetterView::ScrollPosition() const
{
    return scroll;
}

LetterViewResult LetterView::Run()
{
    LetterViewResult result = LetterViewResult::StillOpen;
    switch (state)
    {
    case ViewState::Opening:
        ++stepsSinceOpening;
        PutTheRootAtTheOpeningPlace();
        if (!layout.IsAnimationPlaying(openAnimation) && stepsSinceOpening >= StepsOfTheOpening && !bar.IsChanging())
        {
            ShowTheArrowsOfThePaper();
            state = ViewState::Reading;
        }
        break;

    case ViewState::Reading:
        RunReading();
        break;

    case ViewState::Scrolling:
        RunScrolling();
        break;

    case ViewState::ViewingThePicture:
        RunViewingThePicture();
        break;

    case ViewState::AskingToThrow:
        RunAskingToThrow();
        break;

    case ViewState::Closing:
    case ViewState::Throwing:
        if (!layout.IsAnimationPlaying(closeAnimation))
            result = state == ViewState::Throwing ? LetterViewResult::Thrown : LetterViewResult::Closed;
        break;
    }

    paper.ScrollTo(scroll);
    layout.Calculate();
    return result;
}

void LetterView::RunScrolling()
{
    ++scrollStep;
    float progress = std::min(1.0f, static_cast<float>(scrollStep) / StepsOfAnArrowScroll);
    scroll = scrollStart + (scrollTarget - scrollStart) * Smoothed(progress);
    if (scrollStep < StepsOfAnArrowScroll)
        return;

    ShowTheArrowsOfThePaper();
    state = ViewState::Reading;
}

void LetterView::RunAskingToThrow()
{
    std::optional<DialogAnswer> answer = services.dialogs.TakeAnswer();
    if (!answer)
        return;

    if (answer != DialogAnswer::ButtonA)
    {
        state = ViewState::Reading;
        return;
    }

    services.sounds.Request(ThrowSound);
    layout.StartAnimation(closeAnimation);
    bar.Change(BarChange::BackToTheBoard);
    state = ViewState::Throwing;
}

void LetterView::RunReading()
{
    HandleTheDPad();
    HandleThePresses();
}

void LetterView::HandleTheDPad()
{
    const std::optional<PlayerInput>& player = services.input.Player(0);
    float push = 0;
    if (player && IsHeldDown(*player, ScrollDownButtons))
        push = DPadPush;
    if (player && IsHeldDown(*player, ScrollUpButtons))
        push = -DPadPush;

    scrollSpeed = scrollSpeed * DPadKeptSpeed + push;
    float newScroll = std::clamp(scroll + scrollSpeed, 0.0f, paper.LongestScroll());
    if (std::abs(newScroll - scroll) > SpeedThatSounds)
        services.sounds.Request(ScrollSound);

    scroll = newScroll;
    ShowTheArrowsOfThePaper();
}

void LetterView::HandleThePresses()
{
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
        std::optional<Formats::Vector2> place = PointerPlaceOf(services.input, services.display, playerIndex);
        if (!player || !place || !HasGoneDown(*player, BoardPressButtons))
            continue;

        if (HandlePicturePress(*place) || HandleArrowPress(*place) || HandleBarPress(*place))
            return;
    }
}

bool LetterView::HandlePicturePress(Formats::Vector2 place)
{
    if (!picture || !zoomInAnimation)
        return false;

    for (const Formats::Pane* button : layout.BoundingPanesAt(place))
    {
        if (button->name != PictureButton)
            continue;

        services.sounds.Request(ZoomInSound);
        layout.StartAnimation(*zoomInAnimation);
        state = ViewState::ViewingThePicture;
        return true;
    }
    return false;
}

// The zoomed picture goes back with Back, or with A anywhere.
void LetterView::RunViewingThePicture()
{
    if (layout.IsAnimationPlaying(*zoomInAnimation))
        return;

    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
        if (!player || !HasGoneDown(*player, BoardPressButtons))
            continue;

        std::optional<Formats::Vector2> place = PointerPlaceOf(services.input, services.display, playerIndex);
        if (place && bar.ButtonAt(*place) == BarButton::BackButton)
            bar.FlashButton(BarButton::BackButton);
        services.sounds.Request(ZoomOutSound);
        layout.StartAnimation(*zoomOutAnimation);
        state = ViewState::Reading;
        return;
    }
}

// The arrow at the bottom moves the paper up to show what follows, the arrow at the top moves it back (0x81363CF8: -300 for B_ArwR).
bool LetterView::HandleArrowPress(Formats::Vector2 place)
{
    for (const Formats::Pane* button : layout.BoundingPanesAt(place))
    {
        if (button->name == DownArrowButton && isDownArrowShown)
        {
            StartScrollingTo(std::min(paper.LongestScroll(), scroll + ScrollOfAnArrow));
            return true;
        }
        if (button->name == UpArrowButton && isUpArrowShown)
        {
            StartScrollingTo(std::max(0.0f, scroll - ScrollOfAnArrow));
            return true;
        }
    }
    return false;
}

bool LetterView::HandleBarPress(Formats::Vector2 place)
{
    std::optional<BarButton> button = bar.ButtonAt(place);
    if (button == BarButton::BackButton)
    {
        bar.FlashButton(BarButton::BackButton);
        services.sounds.Request(UnselectSound);
        layout.StartAnimation(closeAnimation);
        bar.Change(BarChange::BackToTheBoard);
        state = ViewState::Closing;
        return true;
    }

    if (button == BarButton::TrashButton)
    {
        bar.FlashButton(BarButton::TrashButton);
        services.sounds.Request(SoundName::ButtonPushed);
        DialogRequest question;
        question.layout = DialogLayout::TwoButtons;
        question.message = services.messages.Find(EraseQuestionMessage);
        question.buttonAText = services.messages.Find(OkMessage);
        question.buttonBText = services.messages.Find(QuitMessage);
        if (services.dialogs.Open(question))
            state = ViewState::AskingToThrow;
        return true;
    }
    return false;
}

void LetterView::StartScrollingTo(float target)
{
    services.sounds.Request(ScrollSound);
    scrollStart = scroll;
    scrollTarget = target;
    scrollStep = 0;
    state = ViewState::Scrolling;
}

// The arrow towards the top hides at the top of the paper, the arrow towards the bottom at its bottom (0x8139A71C).
void LetterView::ShowTheArrowsOfThePaper()
{
    bool shouldShowDown = scroll < paper.LongestScroll();
    bool shouldShowUp = scroll > 0;
    if (shouldShowDown != isDownArrowShown)
        layout.StartAnimation(shouldShowDown ? downArrowAppears : downArrowGoes);
    if (shouldShowUp != isUpArrowShown)
        layout.StartAnimation(shouldShowUp ? upArrowAppears : upArrowGoes);
    isDownArrowShown = shouldShowDown;
    isUpArrowShown = shouldShowUp;
}

void LetterView::Draw()
{
    paper.Draw();
}

}
