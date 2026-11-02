#include "Channels/MiiChannel/PlazaScreen.h"

#include "Channels/MiiChannel/MiiChannelFiles.h"
#include "Channels/MiiChannel/MiiChannelInput.h"

#include <array>
#include <cmath>
#include <format>

namespace Channels
{

namespace
{

// A Mii that slips on the plaza falls with this sound (8001F7A8, state 10).
constexpr std::string_view SlipSound = "SEQ_DOOL_KOROBI";
constexpr std::string_view FirstHitSound = "SEQ_DOOL_THROW_GND1";

// The parade is almost full from 9501 Miis on and has room again below 9400 (0x80042B54: 0x251D and 0x24B8).
constexpr std::size_t ParadeAlmostFullCount = 9501;
constexpr std::size_t ParadeHasRoomCount = 9400;
constexpr std::string_view BounceSound = "SEQ_DOOL_THROW_GND2";

constexpr std::string_view LayoutName = "squareNigaoe_00";

// The animations that bring the buttons of both sides and the bar at the bottom into the picture (bound by 80041024); the plaza shows
// their last frame.
constexpr std::array<std::string_view, 3> ArrivalAnimations = {"anmSQUMenuActionL", "anmSQUMenuActionR_01", "anmSQUWhistleAction"};
constexpr std::array<std::string_view, 3> ArrivalPanes = {"RootPane", "squButtonNull_07", "squButtonNull_04"};

// The focus of the buttons (the table at 8023B9F0 bound by 80041024): the left buttons with anmSQUMenuFocusL, the right ones with
// anmSQUMenuFocusR, each on its squPrBtnNull pane; the whistle with its icon and its text; each sort button with anmSQUSortFocus. The
// round buttons sound big, the two small ones small and the whistle's the editor's (Assumed).
constexpr std::string_view BigFocusSound = "SEQ_BUT_SELECT_BIG";
constexpr std::string_view SmallFocusSound = "SEQ_BUT_SELECT_SMALL";
constexpr std::string_view WhistleFocusSound = "SEQ_EDIT_BUT_SELECT";
constexpr std::array<FocusedButton, 15> PlazaButtonFocus = {{
    {PlazaScreen::WiiMenuButton, "squPrBtnNull_03", "anmSQUMenuFocusL", SmallFocusSound},
    {PlazaScreen::NewMiiButton, "squPrBtnNull_05", "anmSQUMenuFocusL", BigFocusSound},
    {PlazaScreen::EditMiiButton, "squPrBtnNull_06", "anmSQUMenuFocusL", BigFocusSound},
    {PlazaScreen::EraseMiiButton, "squPrBtnNull_08", "anmSQUMenuFocusL", BigFocusSound},
    {PlazaScreen::HelpButton, "squPrBtnNull_09", "anmSQUMenuFocusL", SmallFocusSound},
    {PlazaScreen::ParadeButton, "squPrBtnNull_07", "anmSQUMenuFocusR", BigFocusSound},
    {PlazaScreen::WiiRemoteButton, "squPrBtnNull_00", "anmSQUMenuFocusR", BigFocusSound},
    {PlazaScreen::WiiFriendButton, "squPrBtnNull_01", "anmSQUMenuFocusR", BigFocusSound},
    {PlazaScreen::DsButton, "squPrBtnNull_02", "anmSQUMenuFocusR", BigFocusSound},
    {PlazaScreen::WhistleButton, PlazaScreen::WhistleButton, "anmSQUWhistleFocus", WhistleFocusSound},
    {PlazaScreen::WhistleButton, PlazaScreen::WhistleButton, "anmSQUWhistleTextFocus", WhistleFocusSound},
    {"squButtonNull_10", "squButtonNull_10", "anmSQUSortFocus", WhistleFocusSound},
    {"squButtonNull_11", "squButtonNull_11", "anmSQUSortFocus", WhistleFocusSound},
    {"squButtonNull_12", "squButtonNull_12", "anmSQUSortFocus", WhistleFocusSound},
    {"squButtonNull_13", "squButtonNull_13", "anmSQUSortFocus", WhistleFocusSound},
}};

// The sort buttons of the bar (squText_10 to _13) and what they arrange.
struct SortButton
{
    std::string_view pane;
    PlazaArrangement arrangement;
    std::string_view sound;
};

constexpr std::array<SortButton, 4> SortButtons = {{
    {"squButtonNull_10", PlazaArrangement::Gender, "SEQ_SYUGOMENU_SORT_SEX"},
    {"squButtonNull_11", PlazaArrangement::Color, "SEQ_SYUGOMENU_SORT_COLOR"},
    {"squButtonNull_12", PlazaArrangement::Favorites, "SEQ_SYUGOMENU_SORT_FAVORITE"},
    {"squButtonNull_13", PlazaArrangement::Alphabetical, "SEQ_SYUGOMENU_SORT_50"},
}};

constexpr std::string_view CountText = "countText_00";

// The words of the whistle: "Arrange" and "Disband".
constexpr std::string_view WhistleText = "squText_14";
constexpr std::string_view DisbandText = "squText_15";
constexpr std::string_view LoadWindow = "loadWindow_00";
constexpr std::string_view LoadText = "squLoadTextRC_00";
constexpr std::string_view BalloonPane = "balAnimNull_00";
constexpr std::string_view BalloonName = "balloonText_00";
constexpr std::string_view BalloonCreator = "balText_00";

// Assumed: how long the plaza shows "Reading..." of a Wii Remote and the search for a Nintendo DS before their answers.
constexpr int RemoteReadingSteps = 60;
constexpr int DsSearchSteps = 300;

// The name balloon stays for 3 seconds after A on a Mii (Assumed).
constexpr int BalloonSteps = 180;

// B held with the hand near an edge scrolls the plaza (Assumed): beyond 70 percent of the way from the middle to the edge.
constexpr float EdgeScrollFraction = 0.7f;

constexpr std::string_view ButtonPushSound = "SEQ_HIROBA_BUT_PUSH";
constexpr std::string_view NewMiiSound = "SEQ_HIROBA_NEW";
constexpr std::string_view EditDropSound = "SEQ_HIROBA_EDIT";
constexpr std::string_view EraseSound = "SEQ_HIROBA_TRUSH";
constexpr std::string_view SendToParadeSound = "SEQ_HIROBA_EDIT";
constexpr std::string_view GrabSound = "SEQ_DOOL_GLIP_ON";
constexpr std::string_view LetGoSound = "SEQ_DOOL_GLIP_OFF";
constexpr std::string_view GroundSound = "SEQ_DOOL_GND";
constexpr std::string_view ZoomInSound = "SEQ_CAMERA_ZOOM_ON";
constexpr std::string_view ZoomOutSound = "SEQ_CAMERA_ZOOM_OFF";
constexpr std::string_view WhistleSound = "SEQ_HIROBA_SYUGO_PUSH";
constexpr std::string_view DisbandSound = "SEQ_SYUGOMENU_KAISAN";
constexpr std::string_view NameSound = "SEQ_DOOL_WINDOW_NORMAL";
constexpr std::string_view FavoriteNameSound = "SEQ_DOOL_WINDOW_FAVORITE";
constexpr std::string_view HelpSound = "SEQ_HIROBA_PUSH_HELP";
constexpr std::string_view WiiMenuSound = "SEQ_HIROBA_BUT_RETURN_TO_IPL";
constexpr std::string_view RemoteSound = "SEQ_HIROBA_BUT_WII_CONTROLLER";
constexpr std::string_view FriendSound = "SEQ_HIROBA_BUT_WII_FRIEND";
constexpr std::string_view DsSound = "SEQ_HIROBA_BUT_DS";

constexpr std::uint8_t Shown = 255;
constexpr std::uint8_t Hidden = 0;

}

PlazaScreen::PlazaScreen(ChannelServices& channelServices, const Layout::LayoutArchive& layouts,
    const Formats::NumberedMessageFile& messages, const MiiFigures& miiFigures, const PlazaGround& plazaGround,
    const Mii::MiiResources& faceParts, MessageWindow& messageWindow, MiiChannelData& channelData, MiiEffects& miiEffects,
    std::uint32_t randomSeed)
    : services(channelServices)
    , texts(messages)
    , figures(miiFigures)
    , effects(miiEffects)
    , ground(plazaGround)
    , message(messageWindow)
    , data(channelData)
    , layout(std::make_unique<SystemMenu::MenuLayout>(channelServices.display, layouts, std::string(LayoutName)))
    , heads(channelServices.display.Device(), faceParts)
    , plaza(randomSeed)
{
    FillTextsFromUserData(*layout, messages);

    for (std::size_t animation = 0; animation < ArrivalAnimations.size(); ++animation)
    {
        std::size_t slot = layout->BindAnimationToPane(std::string(ArrivalAnimations[animation]), ArrivalPanes[animation],
            Layout::PlaybackMode::PlayOnce);
        layout->ShowLastFrameOfAnimation(slot);
    }
    focus = std::make_unique<ButtonFocus>(*layout, channelServices.sounds, PlazaButtonFocus);

    for (const SortButton& sort : SortButtons)
        layout->PaneNamed(sort.pane).isVisible = false;
    layout->PaneNamed(DisbandText).isVisible = false;
    HideLoadWindow();
}

// The Miis come in at new places, so a line of the whistle is over.
void PlazaScreen::Start()
{
    plaza.SetMiis(data.Miis());
    ShowWhistleBar(false);
    shouldTellWhatIsNew = true;
}

void PlazaScreen::ShowSavedMii(std::size_t slot)
{
    std::optional<Mii::MiiRecord> record = data.FindMii(slot);
    if (!record)
        return;

    plaza.UpdateMii({slot, *record});
    plaza.DropIn(slot);
}

std::optional<PlazaRequest> PlazaScreen::TakeRequest()
{
    std::optional<PlazaRequest> taken = request;
    request.reset();
    return taken;
}

std::optional<std::size_t> PlazaScreen::MiiToEdit() const
{
    return miiToEdit;
}

const MiiPlaza& PlazaScreen::Plaza() const
{
    return plaza;
}

MiiPlaza& PlazaScreen::Plaza()
{
    return plaza;
}

Formats::Vector2 PlazaScreen::PictureCenterOf(std::size_t slot) const
{
    const PlazaMii* plazaMii = plaza.FindMii(slot);
    if (plazaMii == nullptr)
        return {0, 0};

    Graphics::Canvas canvas = Canvas();
    Formats::Vector3 middle = {plazaMii->position.x, MiiPlaza::MiiMiddleHeight, plazaMii->position.y - MiiPlaza::MiiMiddleHeight};
    return plaza.PlaceInPicture(middle, canvas.width, canvas.height);
}

Formats::Vector2 PlazaScreen::PlaceOf(std::string_view paneName) const
{
    return layout->PointOfPaneInPicture(paneName, {0, 0}).value_or(Formats::Vector2{0, 0});
}

void PlazaScreen::Update(bool acceptsInput)
{
    layout->Calculate();
    plaza.Update();
    for (const PlazaMomentOfMii& moment : ShowPlazaMoments(effects, plaza, figures))
    {
        if (moment.moment == PlazaMoment::HitsTheGround)
            services.sounds.Request(FirstHitSound);
        if (moment.moment == PlazaMoment::BouncesOnTheGround)
            services.sounds.Request(BounceSound);
    }
    if (plaza.SlipsInLastStep() > 0)
        services.sounds.Request(SlipSound);

    heads.Prepare(HeadsOfMiis(plaza));

    std::string count = std::format("{:03}", plaza.Miis().size());
    layout->SetText(CountText, std::u16string(count.begin(), count.end()));
    UpdateNameBalloon();
    UpdateLoadWindow();

    HandleMessageAnswer();
    bool isQuiet = !IsBlocked(acceptsInput) && !plaza.HeldMii() && pending == Pending::Nothing;
    if (isQuiet && shouldTellWhatIsNew)
        TellWhatIsNew();

    bool isBlocked = IsBlocked(acceptsInput);
    bool takesInput = !isBlocked && !plaza.HeldMii();
    focus->Update(takesInput ? ReadPlayerOne(services).pointer : std::nullopt);
    if (isBlocked)
        return;

    HandleHand();
    if (plaza.HeldMii())
        return;

    HandleCamera();
    HandleButtons();
}

// No button reacts while a message or a dialog is over the plaza, has just closed with the press of this step, or the load window shows.
bool PlazaScreen::IsBlocked(bool acceptsInput) const
{
    return !acceptsInput || message.IsOpen() || loadSteps > 0;
}

void PlazaScreen::Draw()
{
    DrawWhiteBackground(services.display.Device(), Canvas());
    figures.DrawPlaza(plaza, ground, heads);
    effects.DrawAmongModels(plaza.ViewMatrix(), plaza.ProjectionMatrix());
    layout->Draw();
}

void PlazaScreen::HandleButtons()
{
    if (HandleLeftButtons())
        return;
    if (HandleRightButtons())
        return;
    HandleWhistleBar();
}

// New Mii, Edit Mii, Erase Mii, Help and Wii Menu; true when one was pressed.
bool PlazaScreen::HandleLeftButtons()
{
    if (IsPressed(NewMiiButton))
    {
        services.sounds.Request(NewMiiSound);
        if (data.IsFull())
            message.ShowNotice(MiiMessage::PlazaIsFullCannotCreate);
        else
            request = PlazaRequest::MakeNewMii;
        return true;
    }

    if (IsPressed(EditMiiButton))
    {
        services.sounds.Request(ButtonPushSound);
        message.ShowNotice(MiiMessage::EditHint);
        return true;
    }

    if (IsPressed(EraseMiiButton))
    {
        services.sounds.Request(ButtonPushSound);
        message.ShowNotice(MiiMessage::EraseHint);
        return true;
    }

    if (IsPressed(HelpButton))
    {
        services.sounds.Request(HelpSound);
        message.ShowNotice(MiiMessage::ControlsHelp);
        return true;
    }

    if (IsPressed(WiiMenuButton))
    {
        services.sounds.Request(WiiMenuSound);
        pending = Pending::ReturnToWiiMenu;
        message.AskQuestion(MiiMessage::ReturnToWiiMenu);
        return true;
    }
    return false;
}

// Mii Parade and the transfers: Wii Remote, Wii Friend and DS; true when one was pressed.
bool PlazaScreen::HandleRightButtons()
{
    if (IsPressed(ParadeButton))
    {
        services.sounds.Request(ButtonPushSound);
        request = PlazaRequest::ShowParade;
        return true;
    }

    if (IsPressed(WiiRemoteButton))
    {
        services.sounds.Request(RemoteSound);
        StartReadingTheRemote();
        return true;
    }

    // Without WiiConnect24 there is no list of Wii Friends.
    if (IsPressed(WiiFriendButton))
    {
        services.sounds.Request(FriendSound);
        message.ShowNotice(MiiMessage::FriendsNotRegistered);
        return true;
    }

    if (IsPressed(DsButton))
    {
        services.sounds.Request(DsSound);
        AskToBeginDs();
        return true;
    }
    return false;
}

// The whistle shows or hides the sort buttons, or disbands the lines; a sort button lines the Miis up.
void PlazaScreen::HandleWhistleBar()
{
    if (IsPressed(WhistleButton))
    {
        if (isArranged)
        {
            services.sounds.Request(DisbandSound);
            plaza.Arrange(PlazaArrangement::Disband);
            ShowWhistleBar(false);
            return;
        }

        services.sounds.Request(WhistleSound);
        ShowWhistleBar(!areSortButtonsShown);
        return;
    }

    if (!areSortButtonsShown)
        return;

    for (const SortButton& sort : SortButtons)
    {
        if (!IsPressed(sort.pane))
            continue;

        services.sounds.Request(sort.sound);
        plaza.Arrange(sort.arrangement);
        ShowWhistleBar(false);
        isArranged = true;
        layout->PaneNamed(WhistleText).isVisible = false;
        layout->PaneNamed(DisbandText).isVisible = true;
        return;
    }
}

// The sort buttons show or hide, and the whistle says "Arrange" again.
void PlazaScreen::ShowWhistleBar(bool showsSortButtons)
{
    areSortButtonsShown = showsSortButtons;
    isArranged = false;
    for (const SortButton& sort : SortButtons)
        layout->PaneNamed(sort.pane).isVisible = showsSortButtons;
    layout->PaneNamed(DisbandText).isVisible = false;
    layout->PaneNamed(WhistleText).isVisible = true;
}

// A and B together grab the Mii under the hand; letting go of either drops it, on a button or on the ground. A alone shows the name.
void PlazaScreen::HandleHand()
{
    PlayerOneInput input = ReadPlayerOne(services);
    constexpr Input::ButtonMask GrabButtons = Input::RemoteButtons::A | Input::RemoteButtons::B;
    Graphics::Canvas canvas = Canvas();

    if (plaza.HeldMii())
    {
        if (!input.pointer)
            return;

        if (!input.IsHeld(GrabButtons))
        {
            DropHeldMii(*input.pointer);
            return;
        }
        plaza.MoveHeldMii(plaza.GroundPointAt(*input.pointer, canvas.width, canvas.height));
        return;
    }

    if (!input.pointer)
        return;

    bool grabsNow = input.IsHeld(GrabButtons) && input.WasPressed(GrabButtons);
    std::optional<std::size_t> pointedMii = plaza.MiiAt(*input.pointer, canvas.width, canvas.height);
    if (grabsNow && pointedMii)
    {
        plaza.Grab(*pointedMii);
        balloonMii.reset();
        services.sounds.Request(GrabSound);
        return;
    }

    if (input.WasPressed(Input::RemoteButtons::A) && !input.IsHeld(Input::RemoteButtons::B) && pointedMii)
        ShowNameBalloon(*pointedMii);
}

void PlazaScreen::HandleCamera()
{
    PlayerOneInput input = ReadPlayerOne(services);
    if (input.WasPressed(Input::RemoteButtons::Plus) && plaza.ZoomIn())
        services.sounds.Request(ZoomInSound);
    if (input.WasPressed(Input::RemoteButtons::Minus) && plaza.ZoomOut())
        services.sounds.Request(ZoomOutSound);

    Formats::Vector2 scroll = {0, 0};
    if (input.IsHeld(Input::RemoteButtons::Left))
        scroll.x -= 1;
    if (input.IsHeld(Input::RemoteButtons::Right))
        scroll.x += 1;
    if (input.IsHeld(Input::RemoteButtons::Up))
        scroll.y -= 1;
    if (input.IsHeld(Input::RemoteButtons::Down))
        scroll.y += 1;

    bool scrollsWithHand = input.IsHeld(Input::RemoteButtons::B) && !input.IsHeld(Input::RemoteButtons::A) && input.pointer;
    if (scrollsWithHand)
    {
        Graphics::Canvas canvas = Canvas();
        float across = input.pointer->x / (canvas.width / 2);
        float upward = input.pointer->y / (canvas.height / 2);
        if (std::abs(across) > EdgeScrollFraction)
            scroll.x += across > 0 ? 1.0f : -1.0f;
        if (std::abs(upward) > EdgeScrollFraction)
            scroll.y += upward > 0 ? -1.0f : 1.0f;
    }

    if (scroll.x != 0 || scroll.y != 0)
        plaza.ScrollCamera(scroll);
}

// Only a question of the plaza's own takes its answer: the window serves the whole channel.
void PlazaScreen::HandleMessageAnswer()
{
    if (pending == Pending::Nothing)
        return;

    std::optional<MessageAnswer> answer = message.TakeAnswer();
    if (!answer)
        return;

    Pending answered = pending;
    pending = Pending::Nothing;
    switch (answered)
    {
    case Pending::Erase:
        if (*answer == MessageAnswer::Yes && miiToErase)
        {
            services.sounds.Request(EraseSound);
            data.DeleteMii(*miiToErase);
            plaza.RemoveMii(*miiToErase);
        }
        miiToErase.reset();
        return;
    case Pending::ReturnToWiiMenu:
        if (*answer == MessageAnswer::Yes)
            services.wiiMenu.Request();
        return;
    case Pending::BeginDs:
        if (*answer == MessageAnswer::Yes)
        {
            ShowLoadWindow(MiiMessage::WaitingForDs);
            loadSteps = DsSearchSteps;
            messageAfterLoad = MiiMessage::NoDsFound;
        }
        return;
    case Pending::ParadeAnnouncements:
        data.SetFlag(ChannelFlag::ParadeAnnounced, true);
        return;
    case Pending::ParadeAlmostFull:
        data.SetFlag(ChannelFlag::ParadeAlmostFullTold, true);
        return;
    case Pending::SendToParade:
        if (*answer == MessageAnswer::Yes && miiToSend)
        {
            services.sounds.Request(SendToParadeSound);
            data.SendToParade(*miiToSend);
            plaza.RemoveMii(*miiToSend);
            shouldTellWhatIsNew = true;
        }
        miiToSend.reset();
        return;
    case Pending::Nothing:
        return;
    }
}

// Once the plaza is quiet it announces the Mii Parade, once (0x80043920), and tells once that the parade is almost full; that warning
// may come again after the parade has had room (0x80042B54).
void PlazaScreen::TellWhatIsNew()
{
    shouldTellWhatIsNew = false;
    if (!data.HasFlag(ChannelFlag::ParadeAnnounced))
    {
        pending = Pending::ParadeAnnouncements;
        message.ShowNoticePages({{MiiMessage::ParadeAnnouncementFirst, MiiMessage::Next, MiiMessage::ParadeAnnouncementTitle},
            {MiiMessage::ParadeAnnouncementFirst + 1, MiiMessage::Next}, {MiiMessage::ParadeAnnouncementFirst + 2, MiiMessage::Next},
            {MiiMessage::ParadeAnnouncementFirst + 3, MiiMessage::Next}, {MiiMessage::ParadeAnnouncementLast, MiiMessage::Ok}});
        return;
    }

    std::size_t paradeCount = data.ParadeMiis().size();
    bool isTold = data.HasFlag(ChannelFlag::ParadeAlmostFullTold);
    if (paradeCount >= ParadeAlmostFullCount && !isTold)
    {
        pending = Pending::ParadeAlmostFull;
        message.ShowNotice(MiiMessage::ParadeAlmostFull);
        return;
    }
    if (paradeCount < ParadeHasRoomCount && isTold)
        data.SetFlag(ChannelFlag::ParadeAlmostFullTold, false);
}

void PlazaScreen::DropHeldMii(Formats::Vector2 pointer)
{
    std::optional<std::size_t> held = plaza.HeldMii();
    plaza.Drop();
    services.sounds.Request(LetGoSound);
    if (!held)
        return;

    std::optional<Mii::MiiRecord> record = data.FindMii(*held);
    if (layout->IsPointInPane(EditMiiButton, pointer) && record)
    {
        DropOnEditMii(*held, *record);
        return;
    }

    if (layout->IsPointInPane(EraseMiiButton, pointer))
    {
        miiToErase = *held;
        pending = Pending::Erase;
        message.AskQuestion(MiiMessage::EraseThisMii, MiiMessage::EraseWord, MiiMessage::DoNotEraseWord);
        return;
    }

    if (layout->IsPointInPane(ParadeButton, pointer) && record)
    {
        DropOnParade(*held, *record);
        return;
    }

    if (layout->IsPointInPane(WiiRemoteButton, pointer))
    {
        StartReadingTheRemote();
        return;
    }

    if (layout->IsPointInPane(WiiFriendButton, pointer))
    {
        message.ShowNotice(MiiMessage::FriendsNotRegistered);
        return;
    }

    if (layout->IsPointInPane(DsButton, pointer))
    {
        AskToBeginDs();
        return;
    }

    services.sounds.Request(GroundSound);
}

// Only the Miis of this console can be edited, and no special Mii.
void PlazaScreen::DropOnEditMii(std::size_t slot, const Mii::MiiRecord& record)
{
    if (!Mii::WasMadeOnConsole(record, data.ConsoleSystemId()) || Mii::IsSpecialMii(record))
    {
        message.ShowNotice(MiiMessage::CannotEditOthersMiis);
        return;
    }
    services.sounds.Request(EditDropSound);
    miiToEdit = slot;
    request = PlazaRequest::EditMii;
}

// "Send to Mii Parade": the parade keeps Miis that the plaza has no room for, without their birthday and creator.
void PlazaScreen::DropOnParade(std::size_t slot, const Mii::MiiRecord& record)
{
    if (Mii::IsSpecialMii(record))
    {
        message.ShowNotice(MiiMessage::SpecialMiisCannotJoinParade);
        return;
    }
    miiToSend = slot;
    pending = Pending::SendToParade;
    message.AskQuestion(MiiMessage::SendToParadeQuestion);
}

// The virtual Wii Remote has no memory for Miis: reading it fails as a remote that does not answer (message 65).
void PlazaScreen::StartReadingTheRemote()
{
    ShowLoadWindow(MiiMessage::ReadingRemote);
    loadSteps = RemoteReadingSteps;
    messageAfterLoad = MiiMessage::RemoteCommunicationError;
}

void PlazaScreen::AskToBeginDs()
{
    pending = Pending::BeginDs;
    message.AskQuestion(MiiMessage::BeginDsCommunication);
}

void PlazaScreen::ShowNameBalloon(std::size_t slot)
{
    std::optional<Mii::MiiRecord> record = data.FindMii(slot);
    if (!record)
        return;

    balloonMii = slot;
    balloonSteps = BalloonSteps;
    layout->SetText(BalloonName, record->name);
    layout->SetText(BalloonCreator, record->creatorName);
    services.sounds.Request(record->isFavorite ? FavoriteNameSound : NameSound);
}

// The name balloon follows its Mii above its head until its time is over or the Mii is gone.
void PlazaScreen::UpdateNameBalloon()
{
    Formats::Pane& balloon = layout->PaneNamed(BalloonPane);
    balloon.isVisible = balloonMii.has_value();
    if (!balloonMii)
        return;

    const PlazaMii* plazaMii = plaza.FindMii(*balloonMii);
    --balloonSteps;
    if (plazaMii == nullptr || balloonSteps <= 0)
    {
        balloonMii.reset();
        return;
    }

    Graphics::Canvas canvas = Canvas();
    float aboveTheHead = MiiPlaza::MiiMiddleHeight * 2;
    Formats::Vector3 above = {plazaMii->position.x, aboveTheHead, plazaMii->position.y - aboveTheHead};
    Formats::Vector2 place = services.display.CanvasToRootPaneSpace(plaza.PlaceInPicture(above, canvas.width, canvas.height));
    balloon.translation.x = place.x;
    balloon.translation.y = place.y;
}

// The load window shows its wait, then the message that ends it.
void PlazaScreen::UpdateLoadWindow()
{
    if (loadSteps <= 0)
        return;

    --loadSteps;
    if (loadSteps > 0)
        return;

    HideLoadWindow();
    message.ShowNotice(messageAfterLoad);
}

void PlazaScreen::ShowLoadWindow(std::uint32_t messageNumber)
{
    layout->SetText(LoadText, MessageText(texts, messageNumber));
    layout->PaneNamed(LoadWindow).alpha = Shown;
}

void PlazaScreen::HideLoadWindow()
{
    layout->PaneNamed(LoadWindow).alpha = Hidden;
}

bool PlazaScreen::IsPressed(std::string_view buttonPane) const
{
    PlayerOneInput input = ReadPlayerOne(services);
    if (!input.pointer || !input.WasPressed(Input::RemoteButtons::A) || input.IsHeld(Input::RemoteButtons::B))
        return false;

    return layout->IsPointInPane(buttonPane, *input.pointer);
}

Graphics::Canvas PlazaScreen::Canvas() const
{
    return services.display.GetCanvas();
}

}
