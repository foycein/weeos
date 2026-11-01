#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace Channels
{

// The texts of the Opera browser inside the Internet Channel, a language file such as english.lng: under [Translation] one line
// id="text" for every text. A page asks for a text by its name, for example S_START; the id is the hash of the name that the
// browser uses: h = h * 33 + character, starting at 5381, read as a signed 32 bit number.
class OperaLanguageFile
{
public:
    // The file is UTF-8. Throws a ChannelError when it has no section [Translation].
    explicit OperaLanguageFile(Formats::ByteView bytes);

    // The text with this name, its escapes \n, \" and \\ resolved. Throws a ChannelError when the file has none.
    const std::u16string& Find(std::string_view name) const;

    // The same by the id, for a text that a program asks for by its id.
    const std::u16string& FindById(std::int32_t id) const;

    static std::int32_t IdOf(std::string_view name);

private:
    void ReadLine(std::string_view line);

    std::map<std::int32_t, std::u16string> texts;
};

}
