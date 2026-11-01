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

constexpr std::string_view LayoutName = "squareNigaoe_00";

// The animations that bring the buttons of both sides and the bar at the bottom into the picture (bound by 80041024); the plaza shows
// their last frame.
constexpr std::array<std::string_view, 3> ArrivalAnimations = {"anmSQUMenuActionL", "anmSQUMenuActionR_01", "anmSQUWhistleAction"};
constexpr std::array<std::string_view, 3> ArrivalPanes = {"RootPane", "squButtonNull_07", "squButtonNull_04"};

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

constexpr Formats::Rgba8Color BackgroundColor = {255, 255, 255, 255};
constexpr std::string_view CountText = "countText_00";
constexpr std::string_view LoadWindow = "loadWindow_00";
constexpr std::string_view LoadText = "squLoadTextRC_00";
constexpr std::string_view BalloonPane = "balAnimNull_00";
constexpr std::string_view BalloonName = "balloonText_00";
constexpr std::string_view BalloonCreator = "balText_00";

constexpr std::uint32_t EditHint = 7;
constexpr std::uint32_t EraseHint = 31;
constexpr std::uint32_t EraseWord = 86;
constexpr std::uint32_t DoNotEraseWord = 85;
constexpr std::uint32_t RemoteCommunicationError = 65;
constexpr std::uint32_t ReadingRemote = 101501;
constexpr std::uint32_t WaitingForDs = 101504;

// Assumed (CHN-05, CHN-68): how long the plaza shows "Reading..." of a Wii Remote and the search for a Nintendo DS before their answers.
constexpr int RemoteReadingSteps = 60;
constexpr int DsSearchSteps = 300;

// The name balloon stays for 3 seconds after A on a Mii (Assumed, CHN-66).
constexpr int BalloonSteps = 180;

// A Mii's middle in the picture: 80 units above its feet, leaning back with the plaza's 45 degrees.
constexpr float MiddleHeight = 80.0f * 0.70710678f;

// B held with the hand near an edge scrolls the plaza (Assumed, CHN-62): beyond 70 percent of the way from the middle to the edge.
constexpr float EdgeScrollFraction = 0.7f;

constexpr std::string_view ButtonPushSound = "SEQ_HIROBA_BUT_PUSH";
constexpr std::string_view NewMiiSound = "SEQ_HIROBA_NEW";
constexpr std::string_view EditDropSound = "SEQ_HIROBA_EDIT";
constexpr std::string_view EraseSound = "SEQ_HIROBA_TRUSH";
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

}

PlazaScreen::PlazaScreen(ChannelServices& channelServices, const Layout::LayoutArchive& layouts,
    const Formats::NumberedMessageFile& messages, const MiiFigures& miiFigures, const PlazaGround& plazaGround,
    const Mii::MiiResources& faceParts, MessageWindow& messageWindow, MiiChannelData& channelData, std::uint32_t randomSeed)
    : services(channelServices)
    , texts(messages)
    , figures(miiFigures)
    , ground(plazaGround)
    , message(messageWindow)
    , data(channelData)
    , layout(std::make_unique<SystemMenu::MenuLayout>(channelServices.display, layouts, std::string(LayoutName)))
    , heads(channelServices.display.Device(), faceParts)
    , plaza(channelServices.display.GetScreenShape() == Platform::ScreenShape::SixteenByNine, randomSeed)
{
    FillTextsFromUserData(*layout, messages);

    for (std::size_t animation = 0; animation < ArrivalAnimations.size(); ++animation)
    {
        std::size_t slot = layout->BindAnimationToPane(std::string(ArrivalAnimations[animation]), ArrivalPanes[animation],
            Layout::PlaybackMode::PlayOnce);
        layout->ShowLastFrameOfAnimation(slot);
    }

    for (const SortButton& sort : SortButtons)
        layout->PaneNamed(sort.pane).isVisible = false;
    layout->PaneNamed("squText_15").isVisible = false;
    HideLoadWindow();
}

void PlazaScreen::Start()
{
    plaza.SetMiis(data.Miis());
}

void PlazaScreen::ShowSavedMii(std::size_t slot)
{
    std::optional<Mii::MiiRecord> record = data.FindMii(slot);
    if (record)
        plaza.UpdateMii({slot, *record});
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
    Formats::Vector3 middle = {plazaMii->position.x, MiddleHeight, plazaMii->position.y - MiddleHeight};
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
    if (plaza.SlipsInLastStep() > 0)
        services.sounds.Request(SlipSound);

    heads.Prepare(HeadsOfThePlaza());

    std::string count = std::format("{:03}", plaza.Miis().size());
    layout->SetText(CountText, std::u16string(count.begin(), count.end()));
    UpdateWaits();

    HandleMessageAnswer();
    if (!acceptsInput || message.IsOpen() || loadSteps > 0)
        return;

    HandleHand();
    if (plaza.HeldMii())
        return;

    HandleCamera();
    HandleButtons();
}

// Every Mii keeps its normal face, and while it talks or sleeps the face of that too, so that its mouth and eyes change without waiting.
std::vector<ShownHead> PlazaScreen::HeadsOfThePlaza() const
{
    std::vector<ShownHead> shownHeads;
    for (const PlazaMii& plazaMii : plaza.Miis())
    {
        std::size_t slot = plazaMii.mii.slot;
        shownHeads.push_back({PlazaHeadKey(slot, Mii::MiiExpression::Normal), plazaMii.mii.record, Mii::MiiExpression::Normal});

        bool talks = plazaMii.activity == PlazaActivity::Talking || plazaMii.activity == PlazaActivity::Answering;
        Mii::MiiExpression face = ExpressionOf(plazaMii);
        if (talks)
            face = Mii::MiiExpression::OpenMouth;
        if (face != Mii::MiiExpression::Normal)
            shownHeads.push_back({PlazaHeadKey(slot, face), plazaMii.mii.record, face});
    }
    return shownHeads;
}

// Beyond the ground the plaza is white, the color its edges fade into (Assumed, CHN-71).
void PlazaScreen::Draw()
{
    Graphics::Canvas canvas = Canvas();
    float halfWidth = canvas.width / 2;
    float halfHeight = canvas.height / 2;
    Graphics::Quad background;
    background.positions = {{{-halfWidth, halfHeight}, {halfWidth, halfHeight}, {halfWidth, -halfHeight}, {-halfWidth, -halfHeight}}};
    background.colors.fill(BackgroundColor);
    services.display.Device().DrawQuad(background, Graphics::Material());

    figures.DrawPlaza(plaza, ground, heads);
    layout->Draw();
}

void PlazaScreen::HandleButtons()
{
    if (IsPressed(NewMiiButton))
    {
        services.sounds.Request(NewMiiSound);
        if (data.IsFull())
            message.ShowNotice(MiiMessage::PlazaIsFullCannotCreate);
        else
            request = PlazaRequest::MakeNewMii;
        return;
    }

    if (IsPressed(EditMiiButton))
    {
        services.sounds.Request(ButtonPushSound);
        message.ShowNotice(EditHint);
        return;
    }

    if (IsPressed(EraseMiiButton))
    {
        services.sounds.Request(ButtonPushSound);
        message.ShowNotice(EraseHint);
        return;
    }

    if (IsPressed(HelpButton))
    {
        services.sounds.Request(HelpSound);
        message.ShowNotice(MiiMessage::ControlsHelp);
        return;
    }

    if (IsPressed(WiiMenuButton))
    {
        services.sounds.Request(WiiMenuSound);
        pending = Pending::ReturnToWiiMenu;
        message.AskQuestion(MiiMessage::ReturnToWiiMenu);
        return;
    }

    if (IsPressed(ParadeButton))
    {
        services.sounds.Request(ButtonPushSound);
        if (hasSeenParade)
        {
            message.ShowNotice(MiiMessage::NobodyIsHere);
            return;
        }
        hasSeenParade = true;
        pending = Pending::ParadeWelcome;
        message.ShowNotice(MiiMessage::WelcomeToTheParade);
        return;
    }

    // The virtual Wii Remote has no memory for Miis: reading it fails as a remote that does not answer (message 65).
    if (IsPressed(WiiRemoteButton))
    {
        services.sounds.Request(RemoteSound);
        ShowLoadWindow(ReadingRemote);
        loadSteps = RemoteReadingSteps;
        messageAfterLoad = RemoteCommunicationError;
        return;
    }

    // Without WiiConnect24 there is no list of Wii Friends.
    if (IsPressed(WiiFriendButton))
    {
        services.sounds.Request(FriendSound);
        message.ShowNotice(MiiMessage::FriendsNotRegistered);
        return;
    }

    if (IsPressed(DsButton))
    {
        services.sounds.Request(DsSound);
        pending = Pending::BeginDs;
        message.AskQuestion(MiiMessage::BeginDsCommunication);
        return;
    }

    if (IsPressed(WhistleButton))
    {
        if (isArranged)
        {
            services.sounds.Request(DisbandSound);
            plaza.Arrange(PlazaArrangement::Disband);
            isArranged = false;
            layout->PaneNamed("squText_15").isVisible = false;
            layout->PaneNamed("squText_14").isVisible = true;
            return;
        }

        services.sounds.Request(WhistleSound);
        areSortButtonsShown = !areSortButtonsShown;
        for (const SortButton& sort : SortButtons)
            layout->PaneNamed(sort.pane).isVisible = areSortButtonsShown;
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
        isArranged = true;
        areSortButtonsShown = false;
        for (const SortButton& hidden : SortButtons)
            layout->PaneNamed(hidden.pane).isVisible = false;
        layout->PaneNamed("squText_14").isVisible = false;
        layout->PaneNamed("squText_15").isVisible = true;
        return;
    }
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

    bool grabsNow = input.IsHeld(GrabButtons) && (input.pressed & GrabButtons) != 0;
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
    if ((input.held & Input::RemoteButtons::Left) != 0)
        scroll.x -= 1;
    if ((input.held & Input::RemoteButtons::Right) != 0)
        scroll.x += 1;
    if ((input.held & Input::RemoteButtons::Up) != 0)
        scroll.y -= 1;
    if ((input.held & Input::RemoteButtons::Down) != 0)
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
            ShowLoadWindow(WaitingForDs);
            loadSteps = DsSearchSteps;
            messageAfterLoad = MiiMessage::NoDsFound;
        }
        return;
    case Pending::ParadeWelcome:
        pending = Pending::ParadeExplained;
        message.ShowNotice(MiiMessage::ParadeExplained);
        return;
    case Pending::ParadeExplained:
        message.ShowNotice(MiiMessage::NobodyIsHere);
        return;
    case Pending::Nothing:
        return;
    }
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
        if (!Mii::WasMadeOnConsole(*record, data.ConsoleSystemId()) || Mii::IsSpecialMii(*record))
        {
            message.ShowNotice(MiiMessage::CannotEditOthersMiis);
            return;
        }
        services.sounds.Request(EditDropSound);
        miiToEdit = *held;
        request = PlazaRequest::EditMii;
        return;
    }

    if (layout->IsPointInPane(EraseMiiButton, pointer))
    {
        miiToErase = *held;
        pending = Pending::Erase;
        message.AskQuestion(MiiMessage::EraseThisMii, EraseWord, DoNotEraseWord);
        return;
    }

    if (layout->IsPointInPane(WiiRemoteButton, pointer))
    {
        ShowLoadWindow(ReadingRemote);
        loadSteps = RemoteReadingSteps;
        messageAfterLoad = RemoteCommunicationError;
        return;
    }

    if (layout->IsPointInPane(WiiFriendButton, pointer))
    {
        message.ShowNotice(MiiMessage::FriendsNotRegistered);
        return;
    }

    if (layout->IsPointInPane(DsButton, pointer))
    {
        pending = Pending::BeginDs;
        message.AskQuestion(MiiMessage::BeginDsCommunication);
        return;
    }

    services.sounds.Request(GroundSound);
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

void PlazaScreen::UpdateWaits()
{
    Formats::Pane& balloon = layout->PaneNamed(BalloonPane);
    balloon.isVisible = balloonMii.has_value();
    if (balloonMii)
    {
        const PlazaMii* plazaMii = plaza.FindMii(*balloonMii);
        --balloonSteps;
        if (plazaMii == nullptr || balloonSteps <= 0)
        {
            balloonMii.reset();
        }
        else
        {
            Graphics::Canvas canvas = Canvas();
            Formats::Vector3 above = {plazaMii->position.x, MiddleHeight * 2, plazaMii->position.y - MiddleHeight * 2};
            Formats::Vector2 place = services.display.CanvasToRootPaneSpace(plaza.PlaceInPicture(above, canvas.width, canvas.height));
            balloon.translation.x = place.x;
            balloon.translation.y = place.y;
        }
    }

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
    layout->PaneNamed(LoadWindow).alpha = 255;
}

void PlazaScreen::HideLoadWindow()
{
    layout->PaneNamed(LoadWindow).alpha = 0;
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
