#include "Text/MessageCatalog.h"

#include "Text/TextError.h"

#include <format>
#include <utility>

namespace Text
{

MessageCatalog::MessageCatalog(Formats::MessageFile messageFile)
    : file(std::move(messageFile))
{
}

std::size_t MessageCatalog::MessageCount() const
{
    return file.messages.size();
}

const std::u16string& MessageCatalog::Find(std::uint16_t messageId) const
{
    if (messageId >= file.messages.size())
        throw TextError(std::format("there is no message {}, the file has {} messages", messageId, file.messages.size()));

    return file.messages[messageId];
}

}
