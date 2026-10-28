#pragma once

#include "Formats/MessageFile.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace Text
{

// The texts of one language. The menu asks for a text by its message id, which is the position of the text in the message file.
class MessageCatalog
{
public:
    explicit MessageCatalog(Formats::MessageFile messageFile);

    std::size_t MessageCount() const;

    // Throws a TextError when the file has no message with this id.
    const std::u16string& Find(std::uint16_t messageId) const;

private:
    Formats::MessageFile file;
};

}
