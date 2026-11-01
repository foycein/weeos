#include "Channels/WiiSpeak/ShowPhotosScreen.h"

#include "Channels/Photo/PhotoPictures.h"
#include "Channels/WiiSpeak/WiiSpeakMessageIds.h"

#include <array>

namespace Channels
{

namespace
{

// The buttons of the layout "view" are the panes "<name>B"; Thumbnails ("back"), the turn and the empty middle are not built.
const std::string PreviousButton = "prevB";
const std::string NextButton = "nextB";
const std::string ShowButton = "todayB";
const std::string FinishButton = "slideB";
constexpr std::array<std::string_view, 3> PanesNotBuilt = {"back", "rot", "blank"};

struct ButtonText
{
    std::string_view pane;
    std::uint32_t messageId;
};

constexpr std::array<ButtonText, 4> ButtonTexts = {{
    {"prevTJP", WiiSpeakText::PhotoPrevious},
    {"nextTJP", WiiSpeakText::PhotoNext},
    {"todayTJP", WiiSpeakText::ShowThisPhoto},
    {"slideTJP", WiiSpeakText::PhotoFinish},
}};

// Where the photo is drawn between the two rows of buttons, in layout units.
constexpr PictureBox PhotoBox = {0, 0, 560, 300};

// The sounds of the viewer, by their names in MicChatSound.brsar.
constexpr std::string_view NextPhotoSound = "SE_PHT_PHOTO_NEXT";
constexpr std::string_view ShowPhotoSound = "SE_PHT_PHOTO_SELECT";
constexpr std::string_view FinishSound = "SE_PHT_CANCEL";

}

ShowPhotosScreen::ShowPhotosScreen(WiiSpeakSession& photoSession)
    : session(photoSession)
    , layout(session.services.display, session.files.PhotoLayouts(), "view")
{
    for (const std::string& button : {PreviousButton, NextButton, ShowButton, FinishButton})
        layout.AddButton(button);
    for (const ButtonText& text : ButtonTexts)
        layout.SetText(text.pane, session.Text(text.messageId));
    for (std::string_view pane : PanesNotBuilt)
        layout.SetVisible(pane, false);

    FindThePhotos();
}

std::optional<WiiSpeakScreenName> ShowPhotosScreen::Update()
{
    std::optional<WiiSpeakScreenName> nextScreen;
    if (session.dialog.IsOpen() || waiting != Waiting::Nothing)
    {
        std::optional<int> choice = session.dialog.TakeChoice();
        if (choice && waiting == Waiting::LeavingAfterTheMessage)
            nextScreen = WiiSpeakScreenName::ChatRoom;
        else if (choice && waiting == Waiting::AnswerToFinishing && *choice == 0)
            nextScreen = WiiSpeakScreenName::ChatRoom;
        if (choice)
            waiting = Waiting::Nothing;
    }
    else
    {
        std::optional<std::string> pressed = layout.TakePressedButton(session.services.input);
        if (pressed)
            nextScreen = Press(*pressed);
    }

    layout.Calculate();
    return nextScreen;
}

void ShowPhotosScreen::Draw()
{
    if (picture)
        picture->DrawInside(PhotoBox);
    layout.Draw();
}

std::vector<SceneLayout*> ShowPhotosScreen::ShownLayouts()
{
    return {&layout};
}

std::size_t ShowPhotosScreen::PhotoCount() const
{
    return photos.size();
}

std::size_t ShowPhotosScreen::ShownPhotoNumber() const
{
    return shownPhoto;
}

// The card is searched at once; movies stay out, a chat shows photos only.
void ShowPhotosScreen::FindThePhotos()
{
    Ios::SdCardService& sdCard = session.services.storage.sdCard;
    if (!sdCard.GetStatus().isInserted)
    {
        LeaveWithMessage(WiiSpeakText::InsertTheSdCard);
        return;
    }

    if (sdCard.ListDirectory(Ios::SdPath::Root()).Failed())
    {
        LeaveWithMessage(WiiSpeakText::SdCardUnreadable);
        return;
    }

    for (const PhotoFile& file : SearchSdCard(sdCard).files)
    {
        if (!file.IsMovie())
            photos.push_back(file);
    }

    if (photos.empty())
    {
        LeaveWithMessage(WiiSpeakText::NoPhotosOnTheSdCard);
        return;
    }
    ShowPhoto(0);
}

void ShowPhotosScreen::LeaveWithMessage(std::uint32_t messageId)
{
    session.ShowMessage(messageId, {WiiSpeakText::Ok});
    waiting = Waiting::LeavingAfterTheMessage;
}

// A photo of a kind the viewer cannot read says so, and the viewer stays on it without a picture.
void ShowPhotosScreen::ShowPhoto(std::size_t number)
{
    shownPhoto = number;
    picture.reset();
    photo.reset();

    std::optional<Formats::Bytes> fileBytes = ReadCardFile(session.services.storage.sdCard, photos[number].path);
    if (fileBytes)
        photo = MakeShownPhoto(*fileBytes);

    if (!photo)
    {
        session.ShowMessage(WiiSpeakText::PhotoNotSupported, {WiiSpeakText::Ok});
        return;
    }
    picture = std::make_unique<ShownPicture>(session.services.display.Device(), *photo);
}

std::optional<WiiSpeakScreenName> ShowPhotosScreen::Press(const std::string& button)
{
    if (button == PreviousButton || button == NextButton)
    {
        std::size_t count = photos.size();
        std::size_t next = button == NextButton ? (shownPhoto + 1) % count : (shownPhoto + count - 1) % count;
        session.services.sounds.Request(NextPhotoSound);
        ShowPhoto(next);
        return std::nullopt;
    }

    if (button == ShowButton && photo)
    {
        session.services.sounds.Request(ShowPhotoSound);
        session.SharePhoto(*photo);
        return WiiSpeakScreenName::ChatRoom;
    }

    if (button == FinishButton)
    {
        session.services.sounds.Request(FinishSound);
        session.ShowMessage(WiiSpeakText::FinishPhotoSelection, {WiiSpeakText::Yes, WiiSpeakText::No});
        waiting = Waiting::AnswerToFinishing;
    }
    return std::nullopt;
}

}
