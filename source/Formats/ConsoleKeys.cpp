#include "Formats/ConsoleKeys.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/FormatError.h"
#include "Formats/HexText.h"

#include <algorithm>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace Formats
{

namespace
{

constexpr char NameValueSeparator = '=';
constexpr std::string_view WhiteSpace = " \t\r";

std::string_view TrimWhiteSpace(std::string_view text)
{
    std::size_t first = text.find_first_not_of(WhiteSpace);
    if (first == std::string_view::npos)
        return std::string_view();

    std::size_t last = text.find_last_not_of(WhiteSpace);
    return text.substr(first, last - first + 1);
}

// The values of keys.txt by their names. Each value is taken once, and what is left over at the end is an error.
class KeyFields
{
public:
    explicit KeyFields(std::string_view text)
    {
        while (!text.empty())
        {
            std::size_t lineEnd = text.find('\n');
            if (lineEnd == std::string_view::npos)
            {
                AddLine(text);
                return;
            }

            AddLine(text.substr(0, lineEnd));
            text.remove_prefix(lineEnd + 1);
        }
    }

    template <std::size_t Size>
    std::array<Byte, Size> TakeBytes(const std::string& name)
    {
        Bytes value = TakeValue(name);
        if (value.size() != Size)
            throw FormatError(std::format("keys.txt: \"{}\" has {} bytes instead of {}", name, value.size(), Size));

        std::array<Byte, Size> bytes;
        std::copy(value.begin(), value.end(), bytes.begin());
        return bytes;
    }

    std::uint32_t TakeNumber(const std::string& name)
    {
        std::array<Byte, sizeof(std::uint32_t)> bytes = TakeBytes<sizeof(std::uint32_t)>(name);
        ByteReader reader(bytes, "keys.txt");
        return reader.ReadUint32();
    }

    void RequireNothingLeft() const
    {
        if (!fields.empty())
            throw FormatError(std::format("keys.txt: the name \"{}\" is not known", fields.begin()->first));
    }

private:
    void AddLine(std::string_view line)
    {
        line = TrimWhiteSpace(line);
        if (line.empty())
            return;

        std::size_t separatorPosition = line.find(NameValueSeparator);
        if (separatorPosition == std::string_view::npos)
            throw FormatError(std::format("keys.txt: the line \"{}\" has no equals sign", line));

        std::string name(TrimWhiteSpace(line.substr(0, separatorPosition)));
        std::string_view valueText = TrimWhiteSpace(line.substr(separatorPosition + 1));

        Bytes value = ParseHexText(valueText);
        if (fields.contains(name))
            throw FormatError(std::format("keys.txt: the name \"{}\" is listed twice", name));

        fields.emplace(name, std::move(value));
    }

    Bytes TakeValue(const std::string& name)
    {
        auto field = fields.find(name);
        if (field == fields.end())
            throw FormatError(std::format("keys.txt: the name \"{}\" is missing", name));

        Bytes value = std::move(field->second);
        fields.erase(field);
        return value;
    }

    std::map<std::string, Bytes> fields;
};

}

namespace
{

void AddLine(std::string& text, std::string_view name, ByteView value)
{
    text += std::format("{:<16}= ", name);
    for (Byte byte : value)
        text += std::format("{:02X}", byte);

    text += '\n';
}

std::array<Byte, sizeof(std::uint32_t)> NumberBytes(std::uint32_t number)
{
    ByteWriter writer;
    writer.WriteUint32(number);

    std::array<Byte, sizeof(std::uint32_t)> bytes;
    std::copy(writer.GetBytes().begin(), writer.GetBytes().end(), bytes.begin());
    return bytes;
}

}

Bytes WriteConsoleKeys(const ConsoleKeys& keys)
{
    std::string text;
    AddLine(text, "boot1_hash", keys.boot1Hash);
    AddLine(text, "wii_common_key", keys.commonKey);
    AddLine(text, "console_id", NumberBytes(keys.consoleId));
    AddLine(text, "ecc_private_key", keys.ellipticCurvePrivateKey);
    AddLine(text, "nand_hmac", keys.nandHmacKey);
    AddLine(text, "nand_aes_key", keys.nandAesKey);
    AddLine(text, "prng_key", keys.randomNumberKey);
    AddLine(text, "ng_key_id", NumberBytes(keys.ngKeyId));
    AddLine(text, "ng_signature", keys.ngSignature);
    AddLine(text, "sd_key", keys.sdKey);
    AddLine(text, "sd_iv", keys.sdInitializationVector);
    AddLine(text, "md5_blanker", keys.md5Blanker);
    AddLine(text, "mac_address", keys.macAddress);
    return Bytes(text.begin(), text.end());
}

ConsoleKeys ReadConsoleKeys(ByteView keysTextBytes)
{
    std::string_view text(reinterpret_cast<const char*>(keysTextBytes.data()), keysTextBytes.size());
    KeyFields fields(text);

    ConsoleKeys keys;
    keys.boot1Hash = fields.TakeBytes<Sha1DigestSize>("boot1_hash");
    keys.commonKey = fields.TakeBytes<AesBlockSize>("wii_common_key");
    keys.consoleId = fields.TakeNumber("console_id");
    keys.ellipticCurvePrivateKey = fields.TakeBytes<EllipticCurvePrivateKeySize>("ecc_private_key");
    keys.nandHmacKey = fields.TakeBytes<NandHmacKeySize>("nand_hmac");
    keys.nandAesKey = fields.TakeBytes<AesBlockSize>("nand_aes_key");
    keys.randomNumberKey = fields.TakeBytes<AesBlockSize>("prng_key");
    keys.ngKeyId = fields.TakeNumber("ng_key_id");
    keys.ngSignature = fields.TakeBytes<NgSignatureSize>("ng_signature");
    keys.sdKey = fields.TakeBytes<AesBlockSize>("sd_key");
    keys.sdInitializationVector = fields.TakeBytes<AesBlockSize>("sd_iv");
    keys.md5Blanker = fields.TakeBytes<AesBlockSize>("md5_blanker");
    keys.macAddress = fields.TakeBytes<MacAddressSize>("mac_address");
    fields.RequireNothingLeft();
    return keys;
}

}
