#include "Channels/MiiChannel/ParadeScreen.h"

#include "Channels/MiiChannel/MiiChannelFiles.h"
#include "Channels/MiiChannel/MiiChannelInput.h"

#include <array>
#include <format>

namespace Channels
{

namespace
{

constexpr std::string_view LayoutName = "uraNigaoe_00";

// The buttons come in with anmURAMenuActionL and anmURAMenuActionR (bound by 80058540 to the buttons of 80241FC4); the parade shows their
// last frame. Their focus is anmURAMenuFocusL and anmURAMenuFocusR.
struct ParadeButton
{
    std::string_view buttonPane;
    std::string_view animatedPane;
    std::string_view arrivalAnimation;
};

constexpr std::array<ParadeButton, 5> ParadeButtons = {{
    {ParadeScreen::SendToPlazaButton, "squPrBtnNull_06", "anmURAMenuActionL"},
    {ParadeScreen::EraseButton, "squPrBtnNull_08", "anmURAMenuActionL"},
    {ParadeScreen::PlazaButton, "squPrBtnNull_07", "anmURAMenuActionR"},
    {ParadeScreen::TravelSettingsButton, "squPrBtnNull_00", "anmURAMenuActionR"},
    {ParadeScreen::SpeedButton, "squPrBtnNull_01", "anmURAMenuActionR"},
}};

constexpr std::string_view FocusSound = "SEQ_BUT_SELECT_BIG";
constexpr std::array<FocusedButton, 5> ParadeButtonFocus = {{
    {ParadeScreen::SendToPlazaButton, "squPrBtnNull_06", "anmURAMenuFocusL", FocusSound},
    {ParadeScreen::EraseButton, "squPrBtnNull_08", "anmURAMenuFocusL", FocusSound},
    {ParadeScreen::PlazaButton, "squPrBtnNull_07", "anmURAMenuFocusR", FocusSound},
    {ParadeScreen::TravelSettingsButton, "squPrBtnNull_00", "anmURAMenuFocusR", FocusSound},
    {ParadeScreen::SpeedButton, "squPrBtnNull_01", "anmURAMenuFocusR", FocusSound},
}};

// The count of the parade, "Miis in the parade / Miis it holds" (Assumed, CHN-84).
constexpr std::string_view CountText = "numberText_00";

// The two pictures of "Speed": walking and running.
constexpr std::string_view RunIcon = "squRunIcon_00";
constexpr std::string_view WalkIcon = "squWalkIcon_00";


constexpr std::string_view ToPlazaSound = "SEQ_PARADE_TO_PLAZA";
constexpr std::string_view ButtonPushSound = "SEQ_HIROBA_BUT_PUSH";
constexpr std::string_view GrabSound = "SEQ_DOOL_GLIP_ON";
constexpr std::string_view LetGoSound = "SEQ_DOOL_GLIP_OFF";
constexpr std::string_view EraseSound = "SEQ_HIROBA_TRUSH";
constexpr std::string_view SendSound = "SEQ_HIROBA_NEW";

}

ParadeScreen::ParadeScreen(ChannelServices& channelServices, const Layout::LayoutArchive& layouts,
    const Formats::NumberedMessageFile& messages, const MiiFigures& miiFigures, const PlazaGround& paradeRoad,
    const Mii::MiiResources& faceParts, MessageWindow& messageWindow, MiiChannelData& channelData, MiiEffects& miiEffects,
    std::uint32_t randomSeed)
    : services(channelServices)
    , figures(miiFigures)
    , effects(miiEffects)
    , road(paradeRoad)
    , message(messageWindow)
    , data(channelData)
    , layout(std::make_unique<SystemMenu::MenuLayout>(channelServices.display, layouts, std::string(LayoutName)))
    , heads(channelServices.display.Device(), faceParts)
    , parade(randomSeed, PlazaKind::Parade)
{
    FillTextsFromUserData(*layout, messages);
    for (const ParadeButton& button : ParadeButtons)
    {
        std::size_t slot = layout->BindAnimationToPane(std::string(button.arrivalAnimation), button.animatedPane,
            Layout::PlaybackMode::PlayOnce);
        layout->ShowLastFrameOfAnimation(slot);
    }
    focus = std::make_unique<ButtonFocus>(*layout, channelServices.sounds, ParadeButtonFocus);
    ShowSpeedIcon();
}

void ParadeScreen::Start()
{
    isWayBackAsked = false;
    ShowParadeMiis();
    if (!data.HasFlag(ChannelFlag::ParadeIntroduced))
    {
        isIntroducing = true;
        message.ShowNoticePages({{MiiMessage::ParadeExplained, MiiMessage::Next, MiiMessage::WelcomeToTheParade},
            {MiiMessage::ParadeMiisCome, MiiMessage::Next}, {MiiMessage::ParadeStoresMiisFirst, MiiMessage::Next},
            {MiiMessage::ParadeStoresMiisFirst + 1, MiiMessage::Next}, {MiiMessage::ParadeStoresMiisLast, MiiMessage::Ok}});
        return;
    }

    if (data.ParadeMiis().empty())
        message.ShowNotice(MiiMessage::NobodyIsHere);
}

bool ParadeScreen::TakeWayBackToThePlaza()
{
    bool isAsked = isWayBackAsked;
    isWayBackAsked = false;
    return isAsked;
}

const MiiPlaza& ParadeScreen::Parade() const
{
    return parade;
}

Formats::Vector2 ParadeScreen::PictureCenterOf(std::size_t place) const
{
    const PlazaMii* paradeMii = parade.FindMii(place);
    if (paradeMii == nullptr)
        return {0, 0};

    Graphics::Canvas canvas = Canvas();
    Formats::Vector3 middle = {paradeMii->position.x, MiiPlaza::MiiMiddleHeight, paradeMii->position.y - MiiPlaza::MiiMiddleHeight};
    return parade.PlaceInPicture(middle, canvas.width, canvas.height);
}

Formats::Vector2 ParadeScreen::PlaceOf(std::string_view paneName) const
{
    return layout->PointOfPaneInPicture(paneName, {0, 0}).value_or(Formats::Vector2{0, 0});
}

void ParadeScreen::Update(bool acceptsInput)
{
    layout->Calculate();
    parade.Update();
    ShowPlazaMoments(effects, parade, figures);
    heads.Prepare(HeadsOfMiis(parade));

    std::string count = std::format("{:05}/{:05}", data.ParadeMiis().size(), Mii::HiddenMiiCapacity);
    layout->SetText(CountText, std::u16string(count.begin(), count.end()));

    HandleMessageAnswer();
    bool takesInput = acceptsInput && !message.IsOpen() && !parade.HeldMii();
    focus->Update(takesInput ? ReadPlayerOne(services).pointer : std::nullopt);
    if (!acceptsInput || message.IsOpen())
        return;

    HandleHand();
    if (parade.HeldMii())
        return;

    HandleButtons();
}

void ParadeScreen::Draw()
{
    DrawWhiteBackground(services.display.Device(), Canvas());
    figures.DrawPlaza(parade, road, heads);
    effects.DrawAmongModels(parade.ViewMatrix(), parade.ProjectionMatrix());
    layout->Draw();
}

// The Miis of the parade walk on the road; each is known by its place in the parade's list.
void ParadeScreen::ShowParadeMiis()
{
    std::vector<Mii::MiiRecord> paradeMiis = data.ParadeMiis();
    std::vector<StoredMii> shown;
    for (std::size_t place = 0; place < paradeMiis.size(); ++place)
        shown.push_back({place, paradeMiis[place]});
    parade.SetMiis(shown);
}

void ParadeScreen::HandleButtons()
{
    if (IsPressed(PlazaButton))
    {
        services.sounds.Request(ToPlazaSound);
        isWayBackAsked = true;
        return;
    }

    if (IsPressed(SpeedButton))
    {
        services.sounds.Request(ButtonPushSound);
        parade.SetParadeRunning(!parade.IsParadeRunning());
        ShowSpeedIcon();
        return;
    }

    if (IsPressed(TravelSettingsButton))
    {
        services.sounds.Request(ButtonPushSound);
        message.ShowNotice(MiiMessage::TravelNeedsWiiConnect24);
        return;
    }

    if (IsPressed(SendToPlazaButton))
    {
        services.sounds.Request(ButtonPushSound);
        message.ShowNotice(MiiMessage::GrabHint);
        return;
    }

    if (IsPressed(EraseButton))
    {
        services.sounds.Request(ButtonPushSound);
        message.ShowNotice(MiiMessage::EraseHint);
    }
}

void ParadeScreen::HandleHand()
{
    PlayerOneInput input = ReadPlayerOne(services);
    constexpr Input::ButtonMask GrabButtons = Input::RemoteButtons::A | Input::RemoteButtons::B;
    Graphics::Canvas canvas = Canvas();
    if (!input.pointer)
        return;

    if (parade.HeldMii())
    {
        if (!input.IsHeld(GrabButtons))
        {
            DropHeldMii(*input.pointer);
            return;
        }
        parade.MoveHeldMii(parade.GroundPointAt(*input.pointer, canvas.width, canvas.height));
        return;
    }

    bool grabsNow = input.IsHeld(GrabButtons) && input.WasPressed(GrabButtons);
    std::optional<std::size_t> pointedMii = parade.MiiAt(*input.pointer, canvas.width, canvas.height);
    if (grabsNow && pointedMii)
    {
        parade.Grab(*pointedMii);
        services.sounds.Request(GrabSound);
    }
}

void ParadeScreen::HandleMessageAnswer()
{
    if (isIntroducing && message.TakeAnswer())
    {
        isIntroducing = false;
        data.SetFlag(ChannelFlag::ParadeIntroduced, true);
        if (data.ParadeMiis().empty())
            message.ShowNotice(MiiMessage::NobodyIsHere);
        return;
    }

    if (!miiToErase)
        return;

    std::optional<MessageAnswer> answer = message.TakeAnswer();
    if (!answer)
        return;

    if (*answer == MessageAnswer::Yes)
    {
        services.sounds.Request(EraseSound);
        data.RemoveFromParade(*miiToErase);
        ShowParadeMiis();
    }
    miiToErase.reset();
}

// A Mii dropped on "Send to Mii Plaza" goes back to the plaza (a full plaza keeps it out, message 28); one dropped on "Erase" leaves
// the parade after the question.
void ParadeScreen::DropHeldMii(Formats::Vector2 pointer)
{
    std::optional<std::size_t> held = parade.HeldMii();
    parade.Drop();
    services.sounds.Request(LetGoSound);
    if (!held)
        return;

    if (layout->IsPointInPane(SendToPlazaButton, pointer))
    {
        if (!data.BringBackFromParade(*held))
        {
            message.ShowNotice(MiiMessage::PlazaIsFullCannotCreate);
            return;
        }
        services.sounds.Request(SendSound);
        ShowParadeMiis();
        return;
    }

    if (layout->IsPointInPane(EraseButton, pointer))
    {
        miiToErase = *held;
        message.AskQuestion(MiiMessage::EraseThisMii, MiiMessage::EraseWord, MiiMessage::DoNotEraseWord);
    }
}

void ParadeScreen::ShowSpeedIcon()
{
    bool runs = parade.IsParadeRunning();
    layout->PaneNamed(RunIcon).isVisible = runs;
    layout->PaneNamed(WalkIcon).isVisible = !runs;
}

bool ParadeScreen::IsPressed(std::string_view buttonPane) const
{
    PlayerOneInput input = ReadPlayerOne(services);
    if (!input.pointer || !input.WasPressed(Input::RemoteButtons::A) || input.IsHeld(Input::RemoteButtons::B))
        return false;

    return layout->IsPointInPane(buttonPane, *input.pointer);
}

Graphics::Canvas ParadeScreen::Canvas() const
{
    return services.display.GetCanvas();
}

}
