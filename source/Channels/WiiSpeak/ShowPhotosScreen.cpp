#include "Channels/WiiSpeak/ShowPhotosScreen.h"

#include "Channels/Photo/PhotoLibrary.h"
#include "Channels/Photo/PhotoPictures.h"
#include "Channels/WiiSpeak/WiiSpeakMessageIds.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Channels
{

namespace
{

struct PaneText
{
    std::string_view pane;
    std::uint32_t messageId;
};

constexpr std::array<PaneText, 2> PlaceTitleTexts = {{{"textTJP", WiiSpeakText::ChoosePhotoLocation}, {"backTJP", WiiSpeakText::PhotoBack}}};
constexpr std::array<PaneText, 2> CardButtonTexts = {{{"sdTJP0A", WiiSpeakText::DigitalCameraOrCellPhone}, {"sdTJP0B", WiiSpeakText::SdCardsOnly}}};
constexpr std::array<PaneText, 1> BoardButtonTexts = {{{"wiiTJP", WiiSpeakText::WiiMessageBoardPhotos}}};
constexpr std::array<PaneText, 4> ThumbnailTexts = {{
    {"zoom_outTJP", WiiSpeakText::ZoomOut},
    {"zoom_inTJP", WiiSpeakText::ZoomIn},
    {"backTJP", WiiSpeakText::ChangePhotoLocation},
    {"slideTJP", WiiSpeakText::FinishThumbnails},
}};
constexpr std::array<PaneText, 5> ViewTexts = {{
    {"prevTJP", WiiSpeakText::PhotoPrevious},
    {"nextTJP", WiiSpeakText::PhotoNext},
    {"backTJP", WiiSpeakText::PhotoThumbnails},
    {"todayTJP", WiiSpeakText::ShowThisPhoto},
    {"slideTJP", WiiSpeakText::PhotoFinish},
}};

// The buttons: the bounding panes of the two places, and the "<name>B" areas of the other layouts.
const std::string CardButton = "sdR";
const std::string BoardButton = "wiiR";
const std::string BackButton = "backB";
const std::string FinishButton = "slideB";
const std::string UpButton = "upB";
const std::string DownButton = "downB";
const std::string ZoomInButton = "zoom_inB";
const std::string ZoomOutButton = "zoom_outB";
const std::string PreviousButton = "prevB";
const std::string NextButton = "nextB";
const std::string ShowButton = "todayB";

// The list of the thumbnails as the Photo Channel draws it: the original draws it itself, so these numbers are Assumed (CHN-31).
constexpr float ListTop = 150;
constexpr float ListBottom = -150;
constexpr float ListSideMargin = 40;
constexpr float CellAspect = 3.0f / 4.0f;
constexpr float CellGap = 6;
constexpr std::array<std::size_t, 3> ColumnsOfZoom = {3, 5, 8};
constexpr std::size_t FirstZoom = 1;
constexpr int ThumbnailsMadePerStep = 2;
constexpr Formats::Byte UnreadableGray = 90;
constexpr Formats::Byte Opaque = 255;

// Where one photo is drawn between the two rows of buttons, in layout units.
constexpr PictureBox PhotoBox = {0, 0, 560, 300};

// The sounds of the viewer, by their names in MicChatSound.brsar.
constexpr std::string_view ChoosePlaceSound = "SE_PHT_MENU_SEL";
constexpr std::string_view ScrollSound = "SE_PHT_SCROLL";
constexpr std::string_view SelectPhotoSound = "SE_PHT_SELECT2";
constexpr std::string_view NextPhotoSound = "SE_PHT_PHOTO_NEXT";
constexpr std::string_view ShowPhotoSound = "SE_PHT_PHOTO_SELECT";
constexpr std::string_view CancelSound = "SE_PHT_CANCEL";

template <std::size_t Count>
void SetTexts(SceneLayout& layout, const WiiSpeakSession& session, const std::array<PaneText, Count>& texts)
{
    for (const PaneText& text : texts)
        layout.SetText(text.pane, session.Text(text.messageId));
}

}

ShowPhotosScreen::ShowPhotosScreen(WiiSpeakSession& photoSession)
    : session(photoSession)
    , placeTitle(session.services.display, session.files.PhotoLayouts(), "title")
    , cardButton(session.services.display, session.files.PhotoLayouts(), "title_sd0")
    , boardButton(session.services.display, session.files.PhotoLayouts(), "title_wii0")
    , thumbnailButtons(session.services.display, session.files.PhotoLayouts(), "thumb")
    , viewButtons(session.services.display, session.files.PhotoLayouts(), "view")
    , zoom(FirstZoom)
{
    SetTexts(placeTitle, session, PlaceTitleTexts);
    SetTexts(cardButton, session, CardButtonTexts);
    SetTexts(boardButton, session, BoardButtonTexts);
    SetTexts(thumbnailButtons, session, ThumbnailTexts);
    SetTexts(viewButtons, session, ViewTexts);
    placeTitle.SetVisible("blank", false);
    viewButtons.SetVisible("rot", false);
    viewButtons.SetVisible("blank", false);

    cardButton.AddButton(CardButton);
    boardButton.AddButton(BoardButton);
    placeTitle.AddAreaButton(BackButton);
    for (const std::string& button : {UpButton, DownButton, ZoomInButton, ZoomOutButton, BackButton, FinishButton})
        thumbnailButtons.AddAreaButton(button);
    for (const std::string& button : {PreviousButton, NextButton, BackButton, ShowButton, FinishButton})
        viewButtons.AddAreaButton(button);

    Formats::RgbaImage gray{1, 1, {{UnreadableGray, UnreadableGray, UnreadableGray, Opaque}}};
    unreadableMarker = std::make_unique<ShownPicture>(session.services.display.Device(), gray);
}

std::optional<WiiSpeakScreenName> ShowPhotosScreen::Update()
{
    std::optional<WiiSpeakScreenName> nextScreen;
    if (session.dialog.IsOpen() || waiting != Waiting::Nothing)
    {
        std::optional<int> choice = session.dialog.TakeChoice();
        if (choice && waiting == Waiting::AnswerToFinishing && *choice == 0)
            nextScreen = WiiSpeakScreenName::ChatRoom;
        if (choice)
            waiting = Waiting::Nothing;
    }
    else
    {
        std::optional<std::string> pressed;
        for (SceneLayout* layout : ShownLayouts())
        {
            if (!pressed)
                pressed = layout->TakePressedButton(session.services.input);
        }

        if (stage == Stage::ChoosingThePlace && pressed)
            nextScreen = UpdatePlace(*pressed);
        else if (stage == Stage::Thumbnails)
            UpdateThumbnails(pressed);
        else if (stage == Stage::Viewing && pressed)
            nextScreen = UpdateView(*pressed);
    }

    if (stage == Stage::Thumbnails)
        MakeSomeThumbnails();
    for (SceneLayout* layout : ShownLayouts())
        layout->Calculate();
    return nextScreen;
}

void ShowPhotosScreen::Draw()
{
    if (stage == Stage::Thumbnails)
        DrawThumbnails();
    if (stage == Stage::Viewing && picture)
        picture->DrawInside(PhotoBox);

    for (SceneLayout* layout : ShownLayouts())
        layout->Draw();
}

std::vector<SceneLayout*> ShowPhotosScreen::ShownLayouts()
{
    if (stage == Stage::ChoosingThePlace)
        return {&placeTitle, &cardButton, &boardButton};
    if (stage == Stage::Thumbnails)
        return {&thumbnailButtons};
    return {&viewButtons};
}

ShowPhotosScreen::Stage ShowPhotosScreen::CurrentStage() const
{
    return stage;
}

std::size_t ShowPhotosScreen::PhotoCount() const
{
    return photos.size();
}

std::size_t ShowPhotosScreen::ShownPhotoNumber() const
{
    return shownPhoto;
}

std::size_t ShowPhotosScreen::ColumnsOfThumbnails() const
{
    return ColumnsOfZoom[zoom];
}

std::optional<WiiSpeakScreenName> ShowPhotosScreen::UpdatePlace(const std::string& pressed)
{
    if (pressed == BackButton)
    {
        session.services.sounds.Request(CancelSound);
        return WiiSpeakScreenName::ChatRoom;
    }

    session.services.sounds.Request(ChoosePlaceSound);
    if (pressed == CardButton)
        OpenCardPhotos();
    else if (pressed == BoardButton)
    {
        ChannelStorage& storage = session.services.storage;
        std::vector<ChatPhoto> found =
            FindBoardPhotos(storage.fileSystem, storage.fileSystemCaller, session.services.network.GetWirelessMacAddress());
        OpenThePhotos(std::move(found), WiiSpeakText::NoPhotosOnTheMessageBoard);
    }
    return std::nullopt;
}

void ShowPhotosScreen::OpenCardPhotos()
{
    Ios::SdCardService& sdCard = session.services.storage.sdCard;
    if (!sdCard.GetStatus().isInserted)
    {
        TellAndStay(WiiSpeakText::InsertTheSdCard);
        return;
    }

    if (sdCard.ListDirectory(Ios::SdPath::Root()).Failed())
    {
        TellAndStay(WiiSpeakText::SdCardUnreadable);
        return;
    }
    OpenThePhotos(FindCardPhotos(sdCard), WiiSpeakText::NoPhotosOnTheSdCard);
}

void ShowPhotosScreen::OpenThePhotos(std::vector<ChatPhoto> foundPhotos, std::uint32_t noPhotosMessageId)
{
    if (foundPhotos.empty())
    {
        TellAndStay(noPhotosMessageId);
        return;
    }

    photos = std::move(foundPhotos);
    thumbnails.clear();
    unreadablePhotos.clear();
    firstRow = 0;
    stage = Stage::Thumbnails;
}

void ShowPhotosScreen::TellAndStay(std::uint32_t messageId)
{
    session.ShowMessage(messageId, {WiiSpeakText::Ok});
    waiting = Waiting::Message;
}

void ShowPhotosScreen::AskToFinish()
{
    session.services.sounds.Request(CancelSound);
    session.ShowMessage(WiiSpeakText::FinishPhotoSelection, {WiiSpeakText::Yes, WiiSpeakText::No});
    waiting = Waiting::AnswerToFinishing;
}

void ShowPhotosScreen::UpdateThumbnails(const std::optional<std::string>& pressed)
{
    const SystemMenu::InputManager& input = session.services.input;
    if (pressed == BackButton)
    {
        session.services.sounds.Request(CancelSound);
        stage = Stage::ChoosingThePlace;
    }
    else if (pressed == FinishButton)
        AskToFinish();
    else if (pressed == UpButton || input.AnyPlayerPressed({Input::RemoteButtons::Up, 0}))
        Scroll(-1);
    else if (pressed == DownButton || input.AnyPlayerPressed({Input::RemoteButtons::Down, 0}))
        Scroll(1);
    else if (pressed == ZoomInButton || input.AnyPlayerPressed({Input::RemoteButtons::Plus, 0}))
        ChangeZoom(-1);
    else if (pressed == ZoomOutButton || input.AnyPlayerPressed({Input::RemoteButtons::Minus, 0}))
        ChangeZoom(1);
    else if (std::optional<std::size_t> number = PhotoAtPress())
    {
        session.services.sounds.Request(SelectPhotoSound);
        stage = Stage::Viewing;
        ShowPhoto(*number);
    }
}

std::optional<WiiSpeakScreenName> ShowPhotosScreen::UpdateView(const std::string& pressed)
{
    if (pressed == PreviousButton || pressed == NextButton)
    {
        std::size_t count = photos.size();
        std::size_t next = pressed == NextButton ? (shownPhoto + 1) % count : (shownPhoto + count - 1) % count;
        session.services.sounds.Request(NextPhotoSound);
        ShowPhoto(next);
    }
    else if (pressed == BackButton)
    {
        session.services.sounds.Request(CancelSound);
        firstRow = std::min(shownPhoto / ColumnsOfThumbnails(), RowCount() - 1);
        stage = Stage::Thumbnails;
    }
    else if (pressed == ShowButton && photo)
    {
        session.services.sounds.Request(ShowPhotoSound);
        session.SharePhoto(*photo);
        return WiiSpeakScreenName::ChatRoom;
    }
    else if (pressed == FinishButton)
        AskToFinish();
    return std::nullopt;
}

// A photo of a kind the viewer cannot read says so, and the viewer stays on it without a picture.
void ShowPhotosScreen::ShowPhoto(std::size_t number)
{
    shownPhoto = number;
    picture.reset();
    photo.reset();

    std::optional<Formats::Bytes> fileBytes = ReadChatPhoto(session.services.storage.sdCard, photos[number]);
    if (fileBytes)
        photo = MakeShownPhoto(*fileBytes);

    if (!photo)
    {
        session.ShowMessage(WiiSpeakText::PhotoNotSupported, {WiiSpeakText::Ok});
        return;
    }
    picture = std::make_unique<ShownPicture>(session.services.display.Device(), *photo);
}

void ShowPhotosScreen::MakeSomeThumbnails()
{
    int made = 0;
    for (std::size_t number = FirstShownPhoto(); number < EndOfShownPhotos() && made < ThumbnailsMadePerStep; ++number)
    {
        if (thumbnails.contains(number) || unreadablePhotos.contains(number))
            continue;

        ++made;
        std::optional<Formats::Bytes> fileBytes = ReadChatPhoto(session.services.storage.sdCard, photos[number]);
        std::optional<Formats::RgbaImage> small;
        if (fileBytes)
            small = MakeThumbnail(*fileBytes, PhotoFileKind::Photo);

        if (!small)
        {
            unreadablePhotos.insert(number);
            continue;
        }
        thumbnails[number] = std::make_unique<ShownPicture>(session.services.display.Device(), *small);
    }
}

void ShowPhotosScreen::DrawThumbnails()
{
    for (std::size_t number = FirstShownPhoto(); number < EndOfShownPhotos(); ++number)
    {
        auto thumbnail = thumbnails.find(number);
        if (thumbnail != thumbnails.end())
            thumbnail->second->DrawInside(BoxOfPhoto(number));
        else if (unreadablePhotos.contains(number))
            unreadableMarker->DrawInside(BoxOfPhoto(number));
    }
}

PictureBox ShowPhotosScreen::BoxOfPhoto(std::size_t number) const
{
    std::size_t row = number / ColumnsOfThumbnails() - firstRow;
    std::size_t column = number % ColumnsOfThumbnails();
    float left = -session.services.display.GetCanvas().width / 2 + ListSideMargin;

    PictureBox box;
    box.centerX = left + (static_cast<float>(column) + 0.5f) * CellWidth();
    box.centerY = ListTop - (static_cast<float>(row) + 0.5f) * CellHeight();
    box.width = CellWidth() - CellGap;
    box.height = CellHeight() - CellGap;
    return box;
}

// The thumbnail under the pointer of the player who pressed A.
std::optional<std::size_t> ShowPhotosScreen::PhotoAtPress() const
{
    const SystemMenu::InputManager& input = session.services.input;
    for (int playerIndex = 0; playerIndex < SystemMenu::PlayerCount; ++playerIndex)
    {
        const std::optional<SystemMenu::PlayerInput>& player = input.Player(playerIndex);
        if (!player || !player->state.pointerIsValid || !input.HasPlayerPressed(playerIndex, SystemMenu::SelectButtons))
            continue;

        Formats::Vector2 place = session.services.display.PointerToCanvas(player->state.pointer);
        for (std::size_t number = FirstShownPhoto(); number < EndOfShownPhotos(); ++number)
        {
            PictureBox box = BoxOfPhoto(number);
            bool isInside = std::abs(place.x - box.centerX) <= box.width / 2 && std::abs(place.y - box.centerY) <= box.height / 2;
            if (isInside)
                return number;
        }
    }
    return std::nullopt;
}

float ShowPhotosScreen::CellWidth() const
{
    float listWidth = session.services.display.GetCanvas().width - 2 * ListSideMargin;
    return listWidth / static_cast<float>(ColumnsOfThumbnails());
}

float ShowPhotosScreen::CellHeight() const
{
    return CellWidth() * CellAspect;
}

std::size_t ShowPhotosScreen::VisibleRows() const
{
    return std::max<std::size_t>(1, static_cast<std::size_t>((ListTop - ListBottom) / CellHeight()));
}

std::size_t ShowPhotosScreen::RowCount() const
{
    return (photos.size() + ColumnsOfThumbnails() - 1) / ColumnsOfThumbnails();
}

std::size_t ShowPhotosScreen::FirstShownPhoto() const
{
    return firstRow * ColumnsOfThumbnails();
}

std::size_t ShowPhotosScreen::EndOfShownPhotos() const
{
    return std::min((firstRow + VisibleRows()) * ColumnsOfThumbnails(), photos.size());
}

void ShowPhotosScreen::Scroll(int rows)
{
    std::size_t lastFirstRow = RowCount() > VisibleRows() ? RowCount() - VisibleRows() : 0;
    std::size_t newFirstRow = firstRow;
    if (rows < 0 && firstRow > 0)
        newFirstRow = firstRow - 1;
    if (rows > 0 && firstRow < lastFirstRow)
        newFirstRow = firstRow + 1;
    if (newFirstRow == firstRow)
        return;

    firstRow = newFirstRow;
    session.services.sounds.Request(ScrollSound);
}

// Smaller thumbnails show more columns; the first shown photo stays in the first row.
void ShowPhotosScreen::ChangeZoom(int step)
{
    if (step > 0 && zoom + 1 >= ColumnsOfZoom.size())
        return;
    if (step < 0 && zoom == 0)
        return;

    std::size_t firstPhoto = FirstShownPhoto();
    zoom = step > 0 ? zoom + 1 : zoom - 1;
    firstRow = firstPhoto / ColumnsOfThumbnails();
    session.services.sounds.Request(ScrollSound);
}

}
