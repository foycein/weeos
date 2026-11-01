#include "SystemMenu/ErrorScreen.h"

#include "SystemMenu/MenuError.h"

#include "Formats/Ash0.h"
#include "Formats/ByteReader.h"
#include "Formats/U8Archive.h"

#include <format>

namespace SystemMenu
{

namespace
{

// Where the data lies in the memory image of the main program: the layout archive (packed with ASH0, it holds the layout and the font)
// and the English texts (a message file).
constexpr std::uint32_t LayoutArchiveAddress = 0x8163B7E0;
constexpr std::uint32_t EnglishTextsAddress = 0x81639900;

// The size of a message file stands in its header, after the eight letters of the tag.
constexpr std::size_t MessageFileSizeOffset = 8;

constexpr std::string_view LayoutName = "my_Fatal";
constexpr std::string_view TextBoxName = "TextBox_00";
constexpr std::string_view FontPath = "arc/font/WiiError.brfnt";
constexpr std::string_view FontName = "WiiError.brfnt";

Formats::MessageFile ReadEnglishTexts(const ProgramImage& program)
{
    Formats::ByteView bytes = program.BytesAt(EnglishTextsAddress);

    Formats::ByteReader reader(bytes, "the English texts of the error screen");
    reader.MoveTo(MessageFileSizeOffset);
    std::size_t fileSize = reader.ReadUint32();

    return Formats::ReadMessageFile(bytes.subspan(0, fileSize));
}

}

ErrorScreen::ErrorScreen(Display& menuDisplay, const ProgramImage& program, MenuFonts& menuFonts, Fader& screenFader)
    : display(menuDisplay)
    , fader(screenFader)
    , texts(ReadEnglishTexts(program))
{
    Formats::U8Archive files = Formats::ReadU8Archive(Formats::DecompressAsh0(program.BytesAt(LayoutArchiveAddress)));

    std::optional<Formats::ByteView> font = files.FindFile(FontPath);
    if (!font)
        throw MenuError("The layout of the error screen has no font");

    menuFonts.AddFont(display.Device(), Formats::ReadFont(*font), {FontName});

    archive = std::make_unique<Layout::LayoutArchive>(std::move(files));
    layout = std::make_unique<MenuLayout>(display, *archive, std::string(LayoutName));
}

void ErrorScreen::Show(FatalError reason)
{
    if (state != State::Off)
        return;

    layout->SetText(TextBoxName, texts.messages.at(static_cast<std::size_t>(reason)));
    state = State::Requested;
}

bool ErrorScreen::IsRequested() const
{
    return state != State::Off;
}

bool ErrorScreen::IsShown() const
{
    return state == State::Showing;
}

void ErrorScreen::Update()
{
    if (state == State::Requested)
    {
        fader.StartFadeOut();

        if (fader.GetStatus() == FaderStatus::Covered)
        {
            fader.StartFadeIn();
            state = State::Showing;
        }
    }

    if (state == State::Showing)
        layout->Calculate();
}

void ErrorScreen::Draw()
{
    if (state == State::Showing)
        layout->Draw();
}

}
