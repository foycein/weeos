#include "SystemMenu/HomeMenu/HomeMenuFiles.h"

#include "SystemMenu/MenuError.h"

#include "Formats/Ash0.h"
#include "Formats/ByteReader.h"
#include "Formats/TplTexture.h"
#include "Formats/U8Archive.h"

#include <format>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::string_view LanguageArchivePath = "layout/eng/homeBtn1.ash";
constexpr std::string_view CommonArchivePath = "layout/common/homeBtn1.ash";
constexpr std::string_view ConfigurationPath = "homebutton/config.txt";
constexpr std::string_view TextsPath = "homebutton/home.csv";
constexpr std::string_view SpeakerSoundsPath = "homebutton/SpeakerSe.arc";
constexpr std::string_view ProhibitedIconPath = "homebutton/homeBtnIcon.tpl";

constexpr std::string_view LayoutFileExtension = ".brlyt";
constexpr char ConfigurationSeparator = ',';
constexpr char ButtonAsksMark = '1';

// home.csv holds one row for every message and one column for every language. English is the second column.
constexpr std::size_t LanguageCount = 10;
constexpr std::size_t EnglishColumn = 1;
constexpr std::size_t ReconnectInstructionRow = 0;
constexpr std::size_t DisconnectingRow = 1;
constexpr std::size_t FirstButtonQuestionRow = 2;
constexpr char16_t ByteOrderMark = 0xFEFF;
constexpr char16_t QuotationMark = u'"';

constexpr std::string_view VolumeSoundName = "volume.bwav";

Formats::U8Archive ReadLayoutArchive(const MenuFiles& files, std::string_view path)
{
    return Formats::ReadU8Archive(Formats::DecompressAsh0(files.ReadFile(path)));
}

// The second archive goes behind the first one, so a file is found in the first archive first.
Formats::U8Archive JoinArchives(const Formats::U8Archive& first, const Formats::U8Archive& second)
{
    Formats::U8Archive joined = first;
    joined.bytes.insert(joined.bytes.end(), second.bytes.begin(), second.bytes.end());

    for (Formats::U8Entry entry : second.entries)
    {
        if (!entry.isDirectory)
            entry.dataOffset += first.bytes.size();

        joined.entries.push_back(entry);
    }
    return joined;
}

std::vector<std::string> SplitConfiguration(Formats::ByteView bytes)
{
    std::vector<std::string> fields(1);
    for (Formats::Byte byte : bytes)
    {
        char character = static_cast<char>(byte);
        if (character == ConfigurationSeparator)
            fields.emplace_back();
        else if (character != '\r' && character != '\n' && character != '\0')
            fields.back().push_back(character);
    }
    return fields;
}

void ReadConfiguration(const MenuFiles& files, HomeMenuFiles& homeMenuFiles)
{
    std::vector<std::string> fields = SplitConfiguration(files.ReadFile(ConfigurationPath));
    if (fields.size() < 3 || !fields[0].ends_with(LayoutFileExtension))
        throw MenuError(std::format("{} does not name a layout, its animations and its buttons", ConfigurationPath));

    homeMenuFiles.layoutName = fields[0].substr(0, fields[0].size() - LayoutFileExtension.size());
    homeMenuFiles.animationPrefix = fields[1];
    for (std::size_t field = 2; field < fields.size(); ++field)
        homeMenuFiles.buttonAsks.push_back(!fields[field].empty() && fields[field][0] == ButtonAsksMark);
}

// The texts are UTF-16, most significant byte first. Every text stands between two quotation marks.
std::vector<std::u16string> ReadQuotedTexts(Formats::ByteView bytes)
{
    Formats::ByteReader reader(bytes, TextsPath);
    std::vector<std::u16string> texts;
    bool isInsideText = false;

    while (reader.RemainingSize() >= sizeof(char16_t))
    {
        char16_t character = reader.ReadUint16();
        if (character == ByteOrderMark)
            continue;

        if (character == QuotationMark)
        {
            isInsideText = !isInsideText;
            if (isInsideText)
                texts.emplace_back();
            continue;
        }

        if (isInsideText)
            texts.back().push_back(character);
    }
    return texts;
}

void ReadTexts(const MenuFiles& files, HomeMenuFiles& homeMenuFiles)
{
    std::vector<std::u16string> texts = ReadQuotedTexts(files.ReadFile(TextsPath));
    std::size_t rowCount = texts.size() / LanguageCount;
    if (rowCount <= FirstButtonQuestionRow)
        throw MenuError(std::format("{} holds too few texts", TextsPath));

    homeMenuFiles.texts.reconnectInstruction = texts[ReconnectInstructionRow * LanguageCount + EnglishColumn];
    homeMenuFiles.texts.disconnecting = texts[DisconnectingRow * LanguageCount + EnglishColumn];
    for (std::size_t row = FirstButtonQuestionRow; row < rowCount; ++row)
        homeMenuFiles.texts.buttonQuestions.push_back(texts[row * LanguageCount + EnglishColumn]);
}

// The speaker sounds have no head: they are samples of 16 bits, most significant byte first.
Input::SpeakerSound ReadSpeakerSound(const Formats::U8Archive& sounds, std::string_view name)
{
    std::optional<Formats::ByteView> file = sounds.FindFile(name);
    if (!file)
        throw MenuError(std::format("{} has no file {}", SpeakerSoundsPath, name));

    Formats::ByteReader reader(*file, name);
    Input::SpeakerSound sound;
    while (!reader.IsAtEnd())
        sound.samples.push_back(reader.ReadInt16());

    return sound;
}

void ReadSpeakerSounds(const MenuFiles& files, HomeMenuFiles& homeMenuFiles)
{
    Formats::U8Archive sounds = Formats::ReadU8Archive(files.ReadFile(SpeakerSoundsPath));
    homeMenuFiles.volumeSound = ReadSpeakerSound(sounds, VolumeSoundName);

    for (std::size_t player = 0; player < homeMenuFiles.connectSounds.size(); ++player)
        homeMenuFiles.connectSounds[player] = ReadSpeakerSound(sounds, std::format("connect{}.bwav", player + 1));
}

Formats::RgbaImage ReadProhibitedIcon(const MenuFiles& files)
{
    std::vector<Formats::TplTexture> textures = Formats::ReadTplTextures(files.ReadFile(ProhibitedIconPath));
    if (textures.empty())
        throw MenuError(std::format("{} holds no picture", ProhibitedIconPath));

    return Formats::DecodeTexture(textures.front());
}

}

HomeMenuFiles ReadHomeMenuFiles(const MenuFiles& files)
{
    HomeMenuFiles homeMenuFiles;

    Formats::U8Archive languageArchive = ReadLayoutArchive(files, LanguageArchivePath);
    Formats::U8Archive commonArchive = ReadLayoutArchive(files, CommonArchivePath);
    homeMenuFiles.archive = std::make_unique<Layout::LayoutArchive>(JoinArchives(languageArchive, commonArchive));

    ReadConfiguration(files, homeMenuFiles);
    ReadTexts(files, homeMenuFiles);
    ReadSpeakerSounds(files, homeMenuFiles);
    homeMenuFiles.prohibitedIcon = ReadProhibitedIcon(files);
    return homeMenuFiles;
}

}
