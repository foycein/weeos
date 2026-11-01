#include "Channels/WiiSpeak/WiiSpeakSession.h"

namespace Channels
{

WiiSpeakSession::WiiSpeakSession(ChannelServices& channelServices, const WiiSpeakFiles& titleFiles, const WiiSpeakMessages& titleMessages,
    WiiSpeakDialog& messageWindow, WiiSpeakSaveFile& file)
    : services(channelServices)
    , files(titleFiles)
    , messages(titleMessages)
    , dialog(messageWindow)
    , saveFile(file)
{
}

std::u16string WiiSpeakSession::Text(std::uint32_t messageId, std::u16string_view argument) const
{
    return messages.Text(messageId, argument);
}

void WiiSpeakSession::ShowMessage(std::uint32_t messageId, const std::vector<std::uint32_t>& buttonMessageIds)
{
    std::vector<std::u16string> labels;
    for (std::uint32_t buttonMessageId : buttonMessageIds)
        labels.push_back(Text(buttonMessageId));

    dialog.Show(Text(messageId), labels);
}

}
