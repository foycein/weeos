#include "Channels/WiiSpeak/ChatPhotos.h"

#include "Channels/Photo/PhotoLibrary.h"
#include "Channels/Photo/PhotoPictures.h"

#include "SystemMenu/IosNandFiles.h"
#include "SystemMenu/MenuError.h"
#include "SystemMenu/MessageBoard/MessageStore.h"

namespace Channels
{

std::vector<ChatPhoto> FindCardPhotos(Ios::SdCardService& sdCard)
{
    std::vector<ChatPhoto> photos;
    for (const PhotoFile& file : SearchSdCard(sdCard).files)
    {
        if (!file.IsMovie())
            photos.push_back({file.path, {}});
    }
    return photos;
}

std::vector<ChatPhoto> FindBoardPhotos(Ios::FileSystemService& fileSystem, Ios::Caller caller, const Ios::MacAddress& wirelessAddress)
{
    std::vector<ChatPhoto> photos;
    SystemMenu::IosNandFiles files(fileSystem, caller);
    SystemMenu::MessageStore board(files, wirelessAddress);
    try
    {
        for (SystemMenu::BoardDay day : board.DaysWithRecords())
        {
            for (const SystemMenu::StoredMessage& stored : board.MessagesOfDay(day))
            {
                for (const std::optional<Formats::BoardAttachment>& attachment : stored.message.attachments)
                {
                    if (attachment && attachment->kind == Formats::BoardAttachmentKind::Picture)
                        photos.push_back({std::nullopt, attachment->contents});
                }
            }
        }
    }
    catch (const SystemMenu::MenuError&)
    {
        return {};
    }
    return photos;
}

std::optional<Formats::Bytes> ReadChatPhoto(Ios::SdCardService& sdCard, const ChatPhoto& photo)
{
    if (photo.cardPath)
        return ReadCardFile(sdCard, *photo.cardPath);

    return photo.boardPicture;
}

}
