#include "Channels/WiiSpeak/WiiSpeakSession.h"

#include "Channels/ChannelText.h"
#include "Channels/WiiConnect24Channel.h"
#include "SystemMenu/NetworkStart.h"
#include "Channels/WiiSpeak/WiiSpeakMessageIds.h"

#include <string>

namespace Channels
{

namespace
{

// The words of the keyboard's buttons in the channel's messages; the others keep the English words of sofkeybd.arc.
SystemMenu::KeyboardButtonTexts KeyboardTextsOf(const WiiSpeakMessages& messages)
{
    SystemMenu::KeyboardButtonTexts texts;
    texts.cancel = messages.Text(WiiSpeakText::Cancel);
    texts.confirm = messages.Text(WiiSpeakText::Ok);
    texts.close = messages.Text(WiiSpeakText::KeyboardBack);
    return texts;
}

// The drops fall at random places, different at every start as the original's are (it seeds with the clock).
std::uint32_t RippleSeedFromClock(const ChannelServices& services)
{
    return static_cast<std::uint32_t>(services.consoleTime().time_since_epoch().count());
}

}

WiiSpeakSession::WiiSpeakSession(ChannelServices& channelServices, const WiiSpeakFiles& titleFiles, const WiiSpeakMessages& titleMessages,
    WiiSpeakDialog& messageWindow, WiiSpeakSaveFile& file, WiiSpeakFriendServer& friendServer)
    : services(channelServices)
    , files(titleFiles)
    , messages(titleMessages)
    , dialog(messageWindow)
    , saveFile(file)
    , friends(friendServer)
    , keyboard(channelServices.display, titleFiles.KeyboardLayouts(), KeyboardTextsOf(titleMessages), channelServices.clock,
          channelServices.input, channelServices.sounds)
    , miiFaces(channelServices.display.Device(), titleFiles.FaceParts())
    , ripples(channelServices.display, titleFiles.RippleEffects(), titleFiles.RipplePictures(), RippleSeedFromClock(channelServices))
{
}

WiiSpeakSession::~WiiSpeakSession()
{
    PutThePhotoAway();
}

void WiiSpeakSession::SharePhoto(const Formats::RgbaImage& photo)
{
    PutThePhotoAway();
    Graphics::TextureDescription description;
    description.width = photo.width;
    description.height = photo.height;
    description.pixels = photo.pixels;

    Layout::LoadedTexture texture;
    texture.handle = services.display.Device().CreateTexture(description);
    texture.width = photo.width;
    texture.height = photo.height;
    sharedPhoto = texture;
    isSharedPhotoNew = true;
}

void WiiSpeakSession::PutThePhotoAway()
{
    if (sharedPhoto)
        services.display.Device().DestroyTexture(sharedPhoto->handle);
    sharedPhoto.reset();
    isSharedPhotoNew = false;
}

std::u16string WiiSpeakSession::Text(std::uint32_t messageId, std::u16string_view argument) const
{
    return messages.Text(messageId, argument);
}

void WiiSpeakSession::SetTexts(SceneLayout& layout, std::span<const PaneText> texts) const
{
    for (const PaneText& text : texts)
        layout.SetText(text.pane, Text(text.messageId));
}

// WeeOS has no WiiConnect24: the message comes at once, without a try. Without an address the name of the server is not
// found; with one, the service is not there to answer.
SystemMenu::ServerAnswer WiiSpeakSession::WiiConnect24ServerAnswerOf(const std::string& serverAddress)
{
    if (serverAddress.empty())
        return SystemMenu::ServerAnswer::NameNotFound;

    return SystemMenu::ServerAnswer::NoAnswer;
}

void WiiSpeakSession::ShowWiiConnect24Unreachable()
{
    SystemMenu::NetworkStart start = SystemMenu::StartNetwork(services.network.configuration, services.network.sockets);
    std::uint32_t errorCode = DownloadErrorCodeOfStart(start.startupError);
    if (start.isOnline)
        errorCode = DownloadErrorCodeOfServer(WiiConnect24ServerAnswerOf(services.network.serverAddress));

    std::u16string code = AsciiToText(std::to_string(errorCode));
    dialog.Show(Text(WiiSpeakText::WiiConnect24CannotReachTheInternet, code), {Text(WiiSpeakText::Ok)});
}

void WiiSpeakSession::ShowMessage(std::uint32_t messageId, const std::vector<std::uint32_t>& buttonMessageIds)
{
    std::vector<std::u16string> labels;
    for (std::uint32_t buttonMessageId : buttonMessageIds)
        labels.push_back(Text(buttonMessageId));

    dialog.Show(Text(messageId), labels);
}

}
