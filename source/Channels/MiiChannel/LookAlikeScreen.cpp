#include "Channels/MiiChannel/LookAlikeScreen.h"

#include "Channels/MiiChannel/MiiChannelFiles.h"
#include "Channels/MiiChannel/MiiChannelInput.h"

#include <format>

namespace Channels
{

namespace
{

constexpr std::string_view LayoutName = "selectNigaoe_00";
constexpr std::string_view FirstStageWindow = "windowSelect_01";
constexpr std::string_view GridWindow = "windowSelect_00";
constexpr std::string_view LeaveButton = "frmCan_01";
constexpr std::string_view BackButton = "frmCan_00";
constexpr std::string_view ResetButton = "frmReset_00";
constexpr std::string_view FirstStageHelpButton = "frmHelp_01";
constexpr std::string_view GridHelpButton = "frmHelp_00";
constexpr std::uint32_t FirstFacesHelp = 43;
constexpr std::uint32_t GridHelp = 45;
constexpr std::string_view PickedFacePane = "frmFace_00";

// The nine cards of the grid are the windows card_48 to card_56.
constexpr int FirstGridCard = 48;

constexpr std::uint32_t IsThisTheFace = 500400;
constexpr std::uint32_t YesWord = 500500;
constexpr std::uint32_t NoWord = 500600;

constexpr std::string_view PickSound = "SEQ_EDIT_STAMP_PART_FACE";
constexpr std::string_view UseSound = "SEQ_YES_END_SELECT_FACE";
constexpr std::string_view ResetSound = "SEQ_YES_RETRY_SELECT_FACE";
constexpr std::string_view BackSound = "SEQ_CANCEL";
constexpr std::string_view HelpSound = "SEQ_PUSH_HELP_COMMON";

// The face that was picked keeps its key, so its head is not made again.
constexpr std::size_t PickedFaceKey = 100;

// The channel's own random age (80060518): 4 in 10 children, 5 in 10 adults, 1 in 10 elders.
constexpr int AgeDraws = 10;
constexpr int ChildDraws = 4;
constexpr int ChildOrAdultDraws = 9;

// The rows of the grid around the picked face (800601E8): East Asian above, light in the middle, dark below.
Mii::MiiLook LookOfGridPlace(int place)
{
    constexpr int PlacesPerRow = 3;
    int row = place / PlacesPerRow;
    if (row == 0)
        return Mii::MiiLook::EastAsian;
    if (row == 1)
        return Mii::MiiLook::LightSkin;
    return Mii::MiiLook::DarkSkin;
}

}

LookAlikeScreen::LookAlikeScreen(ChannelServices& channelServices, const Layout::LayoutArchive& layouts,
    const Formats::NumberedMessageFile& messages, const MiiFigures& miiFigures, const Mii::MiiResources& faceParts,
    MessageWindow& messageWindow, std::uint32_t randomSeed)
    : services(channelServices)
    , figures(miiFigures)
    , message(messageWindow)
    , layout(std::make_unique<SystemMenu::MenuLayout>(channelServices.display, layouts, std::string(LayoutName)))
    , heads(channelServices.display.Device(), faceParts)
    , random(randomSeed)
{
    FillTextsFromUserData(*layout, messages);
}

void LookAlikeScreen::Start(Mii::Gender newGender)
{
    gender = newGender;
    isOpen = true;
    isConfirming = false;
    result.reset();
    MakeFirstFaces();
}

bool LookAlikeScreen::IsOpen() const
{
    return isOpen;
}

std::optional<LookAlikeResult> LookAlikeScreen::TakeResult()
{
    std::optional<LookAlikeResult> taken = result;
    result.reset();
    return taken;
}

const std::vector<Mii::MiiRecord>& LookAlikeScreen::Faces() const
{
    return faces;
}

bool LookAlikeScreen::IsRefining() const
{
    return isRefining;
}

std::string LookAlikeScreen::CardPane(int faceNumber) const
{
    if (isRefining)
        return std::format("card_{:02}", faceNumber + FirstGridCard);

    return std::format("card_{:02}", faceNumber);
}

Formats::Vector2 LookAlikeScreen::PlaceOf(std::string_view paneName) const
{
    return layout->PointOfPaneInPicture(paneName, {0, 0}).value_or(Formats::Vector2{0, 0});
}

void LookAlikeScreen::Update(bool acceptsInput)
{
    layout->Calculate();
    if (!isOpen)
        return;

    std::vector<ShownHead> shownFaces;
    for (std::size_t faceNumber = 0; faceNumber < faces.size(); ++faceNumber)
        shownFaces.push_back({faceNumber, faces[faceNumber]});
    if (isRefining)
        shownFaces.push_back({PickedFaceKey, faces[MiddleOfGrid]});
    heads.Prepare(shownFaces);

    if (isConfirming)
    {
        HandleConfirmation();
        return;
    }

    if (!acceptsInput || message.IsOpen())
        return;

    HandleButtons();
}

// "Is this the face you want?": Yes gives the face to the editor, No goes on choosing.
void LookAlikeScreen::HandleConfirmation()
{
    std::optional<MessageAnswer> answer = message.TakeAnswer();
    if (!answer)
        return;

    isConfirming = false;
    if (*answer == MessageAnswer::Yes)
    {
        services.sounds.Request(UseSound);
        Finish({.record = faces[MiddleOfGrid]});
    }
}

void LookAlikeScreen::HandleButtons()
{
    for (int faceNumber = 0; faceNumber < static_cast<int>(faces.size()); ++faceNumber)
    {
        if (IsPressed(CardPane(faceNumber)))
        {
            Pick(faceNumber);
            return;
        }
    }

    if (!isRefining && IsPressed(LeaveButton))
    {
        services.sounds.Request(BackSound);
        Finish({.wasCancelled = true});
        return;
    }

    if (isRefining && (IsPressed(BackButton) || IsPressed(ResetButton)))
    {
        services.sounds.Request(ResetSound);
        MakeFirstFaces();
        return;
    }

    if (IsPressed(isRefining ? GridHelpButton : FirstStageHelpButton))
    {
        services.sounds.Request(HelpSound);
        message.ShowNotice(isRefining ? GridHelp : FirstFacesHelp);
        return;
    }

    if (isRefining && IsPressed(UseThisFaceButton))
    {
        isConfirming = true;
        message.AskQuestion(IsThisTheFace, YesWord, NoWord);
    }
}

void LookAlikeScreen::Draw()
{
    if (!isOpen)
        return;

    layout->Draw();
    Graphics::Canvas canvas = services.display.GetCanvas();
    for (std::size_t faceNumber = 0; faceNumber < faces.size(); ++faceNumber)
    {
        const Mii::MiiHead* head = heads.Find(faceNumber);
        if (head != nullptr)
            figures.DrawHead(*head, RectangleOfPane(*layout, CardPane(static_cast<int>(faceNumber))), canvas);
    }

    const Mii::MiiHead* picked = heads.Find(PickedFaceKey);
    if (isRefining && picked != nullptr)
        figures.DrawHead(*picked, RectangleOfPane(*layout, PickedFacePane), canvas);
}

void LookAlikeScreen::MakeFirstFaces()
{
    isRefining = false;
    faces.clear();
    for (int faceNumber = 0; faceNumber < FirstFaceCount; ++faceNumber)
        faces.push_back(Mii::MakeRandomMii(gender, RandomAge(), Mii::ChooseRandomLook(random), random));
    ShowStage();
}

void LookAlikeScreen::Pick(int faceNumber)
{
    if (isRefining && faceNumber == MiddleOfGrid)
        return;

    services.sounds.Request(PickSound);
    Mii::MiiRecord picked = faces[faceNumber];

    faces.clear();
    for (int place = 0; place < GridFaceCount; ++place)
    {
        if (place == MiddleOfGrid)
            faces.push_back(picked);
        else
            faces.push_back(Mii::MakeRandomMii(gender, Mii::MiiAge::Adult, LookOfGridPlace(place), random));
    }
    isRefining = true;
    ShowStage();
}

void LookAlikeScreen::ShowStage()
{
    layout->PaneNamed(FirstStageWindow).isVisible = !isRefining;
    layout->PaneNamed(GridWindow).isVisible = isRefining;
}

Mii::MiiAge LookAlikeScreen::RandomAge()
{
    int draw = std::uniform_int_distribution<int>(0, AgeDraws - 1)(random);
    if (draw < ChildDraws)
        return Mii::MiiAge::Child;
    if (draw < ChildOrAdultDraws)
        return Mii::MiiAge::Adult;
    return Mii::MiiAge::Elder;
}

bool LookAlikeScreen::IsPressed(std::string_view buttonPane) const
{
    PlayerOneInput input = ReadPlayerOne(services);
    if (!input.pointer || !input.WasPressed(Input::RemoteButtons::A))
        return false;

    return layout->IsPointInPane(buttonPane, *input.pointer);
}

void LookAlikeScreen::Finish(LookAlikeResult finished)
{
    result = finished;
    isOpen = false;
}

}
