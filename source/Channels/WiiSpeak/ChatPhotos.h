#pragma once

#include "Ios/Caller.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Network/MacAddress.h"
#include "Ios/SdCard/SdCardService.h"

#include "Formats/Bytes.h"

#include <optional>
#include <vector>

namespace Channels
{

// A photo that Show Photos can show: a JPEG file of the SD card, or the picture of a letter on the Wii Message Board.
struct ChatPhoto
{
    std::optional<Ios::SdPath> cardPath;
    Formats::Bytes boardPicture;
};

// The photos of the card, found as the Photo Channel finds them; movies stay out, a chat shows photos only.
std::vector<ChatPhoto> FindCardPhotos(Ios::SdCardService& sdCard);

// The pictures of the letters on the Message Board, the oldest day first, read from cdb.vff through the board's store as this
// channel. A board that cannot be read has none.
std::vector<ChatPhoto> FindBoardPhotos(Ios::FileSystemService& fileSystem, Ios::Caller caller, const Ios::MacAddress& wirelessAddress);

// The JPEG bytes of the photo. Nothing when the card cannot be read.
std::optional<Formats::Bytes> ReadChatPhoto(Ios::SdCardService& sdCard, const ChatPhoto& photo);

}
