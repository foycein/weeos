#include "SystemMenu/HomeMenu/HomeMenuFiles.h"

#include "SystemMenu/MenuError.h"

#include "Formats/Ash0.h"
#include "Formats/ByteReader.h"
#include "Formats/Huffman.h"
#include "Formats/Lz77.h"
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

// The copy of the HOME menu that a channel carries, in a folder of its version (HomeButton2, HomeButton3 of the shared content, or
// homebutton of the Internet Channel's own content). In the shared content the archives are packed with LZ77 and the speaker sounds
// with Huffman, and their names say so.
constexpr std::string_view ChannelLanguageArchiveName = "homeBtn_ENG.arc";
constexpr std::string_view ChannelCommonArchiveName = "homeBtn.arc";
constexpr std::string_view ChannelSpeakerSoundsName = "SpeakerSe.arc";
constexpr std::string_view Lz77NamePrefix = "LZ77_";
constexpr std::string_view HuffmanNamePrefix = "Huf8_";
constexpr std::string_view ChannelConfigurationName = "config.txt";
constexpr std::string_view ChannelTextsName = "home.csv";
constexpr std::string_view ChannelProhibitedIconName = "homeBtnIcon.tpl";

constexpr std::string_view LayoutFileExtension = ".brlyt";
constexpr char ConfigurationSeparator = ',';
constexpr char ButtonAsksMark = '1';

// config.txt is one line of fields: the layout file, the start of the animation names, then one field for every big button.
constexpr std::size_t LayoutFileField = 0;
constexpr std::size_t AnimationPrefixField = 1;
constexpr std::size_t FirstButtonField = 2;

// home.csv holds one row for every message and one column for every language. English is the second column.
// The menu's file has ten languages, the copies in the channels seven.
constexpr std::size_t EnglishColumn = 1;
constexpr std::size_t ReconnectInstructionRow = 0;
constexpr std::size_t DisconnectingRow = 1;
constexpr std::size_t FirstButtonQuestionRow = 2;
constexpr char16_t ByteOrderMark = 0xFEFF;
constexpr char16_t QuotationMark = u'"';
constexpr char16_t LineFeed = u'\n';

constexpr std::string_view VolumeSoundName = "volume.bwav";

Formats::ByteView FindFileOfContent(const Formats::U8Archive& content, const std::string& path)
{
    std::optional<Formats::ByteView> file = content.FindFile(path);
    if (!file)
        throw MenuError(std::format("The HOME menu of the channel has no file {}", path));

    return *file;
}

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
        {
            fields.emplace_back();
            continue;
        }

        bool isLineEndOrZero = character == '\r' || character == '\n' || character == '\0';
        if (!isLineEndOrZero)
            fields.back().push_back(character);
    }
    return fields;
}

void ReadConfiguration(Formats::ByteView configuration, HomeMenuFiles& homeMenuFiles)
{
    std::vector<std::string> fields = SplitConfiguration(configuration);
    bool hasAllFields = fields.size() > FirstButtonField && fields[LayoutFileField].ends_with(LayoutFileExtension);
    if (!hasAllFields)
        throw MenuError(std::format("{} does not name a layout, its animations and its buttons", ConfigurationPath));

    const std::string& layoutFile = fields[LayoutFileField];
    homeMenuFiles.layoutName = layoutFile.substr(0, layoutFile.size() - LayoutFileExtension.size());
    homeMenuFiles.animationPrefix = fields[AnimationPrefixField];
    for (std::size_t field = FirstButtonField; field < fields.size(); ++field)
    {
        bool asks = fields[field].starts_with(ButtonAsksMark);
        homeMenuFiles.buttonAsks.push_back(asks);
    }
}

// The texts are UTF-16, most significant byte first. Every text stands between two quotation marks; a line feed outside the
// quotation marks ends a row.
std::vector<std::vector<std::u16string>> ReadQuotedRows(Formats::ByteView bytes)
{
    Formats::ByteReader reader(bytes, TextsPath);
    std::vector<std::vector<std::u16string>> rows(1);
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
                rows.back().emplace_back();
            continue;
        }

        if (isInsideText)
            rows.back().back().push_back(character);
        else if (character == LineFeed)
            rows.emplace_back();
    }
    return rows;
}

const std::u16string& EnglishTextOf(const std::vector<std::u16string>& row)
{
    if (row.size() <= EnglishColumn)
        throw MenuError(std::format("A row of {} has no English text", TextsPath));

    return row[EnglishColumn];
}

void ReadTexts(Formats::ByteView textFile, HomeMenuFiles& homeMenuFiles)
{
    std::vector<std::vector<std::u16string>> rows = ReadQuotedRows(textFile);
    std::erase_if(rows, [](const std::vector<std::u16string>& row) { return row.empty(); });
    if (rows.size() <= FirstButtonQuestionRow)
        throw MenuError(std::format("{} holds too few texts", TextsPath));

    homeMenuFiles.texts.reconnectInstruction = EnglishTextOf(rows[ReconnectInstructionRow]);
    homeMenuFiles.texts.disconnecting = EnglishTextOf(rows[DisconnectingRow]);
    for (std::size_t row = FirstButtonQuestionRow; row < rows.size(); ++row)
        homeMenuFiles.texts.buttonQuestions.push_back(EnglishTextOf(rows[row]));
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

void ReadSpeakerSounds(const Formats::U8Archive& sounds, HomeMenuFiles& homeMenuFiles)
{
    homeMenuFiles.volumeSound = ReadSpeakerSound(sounds, VolumeSoundName);

    for (std::size_t player = 0; player < homeMenuFiles.connectSounds.size(); ++player)
        homeMenuFiles.connectSounds[player] = ReadSpeakerSound(sounds, std::format("connect{}.bwav", player + 1));
}

Formats::RgbaImage ReadProhibitedIcon(Formats::ByteView iconFile)
{
    std::vector<Formats::TplTexture> textures = Formats::ReadTplTextures(iconFile);
    if (textures.empty())
        throw MenuError(std::format("{} holds no picture", ProhibitedIconPath));

    return Formats::DecodeTexture(textures.front());
}

Formats::U8Archive ReadChannelArchive(const Formats::U8Archive& content, std::string_view folder, std::string_view name, bool isPacked)
{
    if (!isPacked)
        return Formats::ReadU8Archive(FindFileOfContent(content, std::format("{}/{}", folder, name)));

    Formats::ByteView packedFile = FindFileOfContent(content, std::format("{}/{}{}", folder, Lz77NamePrefix, name));
    return Formats::ReadU8Archive(Formats::DecompressLz77(packedFile));
}

Formats::U8Archive ReadChannelSpeakerSounds(const Formats::U8Archive& content, std::string_view folder, bool isPacked)
{
    if (!isPacked)
        return Formats::ReadU8Archive(FindFileOfContent(content, std::format("{}/{}", folder, ChannelSpeakerSoundsName)));

    std::string packedName = std::format("{}/{}{}", folder, HuffmanNamePrefix, ChannelSpeakerSoundsName);
    Formats::ByteView packedFile = FindFileOfContent(content, packedName);
    return Formats::ReadU8Archive(Formats::DecompressHuffman(packedFile));
}

}

HomeMenuFiles ReadHomeMenuFiles(const MenuFiles& files)
{
    HomeMenuFiles homeMenuFiles;

    Formats::U8Archive languageArchive = ReadLayoutArchive(files, LanguageArchivePath);
    Formats::U8Archive commonArchive = ReadLayoutArchive(files, CommonArchivePath);
    homeMenuFiles.archive = std::make_unique<Layout::LayoutArchive>(JoinArchives(languageArchive, commonArchive));

    ReadConfiguration(files.ReadFile(ConfigurationPath), homeMenuFiles);
    ReadTexts(files.ReadFile(TextsPath), homeMenuFiles);
    ReadSpeakerSounds(Formats::ReadU8Archive(files.ReadFile(SpeakerSoundsPath)), homeMenuFiles);
    homeMenuFiles.prohibitedIcon = ReadProhibitedIcon(files.ReadFile(ProhibitedIconPath));
    return homeMenuFiles;
}

HomeMenuFiles ReadChannelHomeMenuFiles(const Formats::U8Archive& homeMenuContent, std::string_view folder, bool arePacked,
    std::size_t offeredButtonCount)
{
    auto readFile = [&](std::string_view name) { return FindFileOfContent(homeMenuContent, std::format("{}/{}", folder, name)); };

    HomeMenuFiles homeMenuFiles;
    Formats::U8Archive languageArchive = ReadChannelArchive(homeMenuContent, folder, ChannelLanguageArchiveName, arePacked);
    Formats::U8Archive commonArchive = ReadChannelArchive(homeMenuContent, folder, ChannelCommonArchiveName, arePacked);
    homeMenuFiles.archive = std::make_unique<Layout::LayoutArchive>(JoinArchives(languageArchive, commonArchive));

    ReadConfiguration(readFile(ChannelConfigurationName), homeMenuFiles);
    if (homeMenuFiles.buttonAsks.size() > offeredButtonCount)
        homeMenuFiles.buttonAsks.resize(offeredButtonCount);

    ReadTexts(readFile(ChannelTextsName), homeMenuFiles);
    ReadSpeakerSounds(ReadChannelSpeakerSounds(homeMenuContent, folder, arePacked), homeMenuFiles);
    homeMenuFiles.prohibitedIcon = ReadProhibitedIcon(readFile(ChannelProhibitedIconName));
    return homeMenuFiles;
}

}
