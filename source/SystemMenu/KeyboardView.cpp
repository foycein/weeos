#include "SystemMenu/KeyboardView.h"

#include "SystemMenu/MenuError.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "sofkeybd";

constexpr std::string_view RootPaneName = "RootPane";
constexpr std::string_view TitlePane = "T_title_text";
constexpr std::string_view FieldTextPane = "T_2l_TextBox";

// The messages that name the buttons.
constexpr std::uint16_t OkMessage = 46;
constexpr std::uint16_t CancelMessage = 96;
constexpr std::uint16_t OthersMessage = 70;
constexpr std::uint16_t BackMessage = 35;
constexpr std::uint16_t NextMessage = 163;
constexpr std::uint16_t CloseMessage = 164;

// A key that the field would not take shows this opaque.
constexpr std::uint8_t NotAllowedKeyAlpha = 90;
constexpr std::uint8_t FullAlpha = 255;

constexpr Formats::Rgba8Color CaretColor = {0, 0, 0, 255};
constexpr Formats::Rgba8Color SelectionColor = {52, 190, 237, 110};
constexpr float CaretHalfWidth = 1;

std::string AnimationEnding(KeyAnimation animation)
{
    switch (animation)
    {
    case KeyAnimation::FocusIn:
        return "Focus-IN";
    case KeyAnimation::FocusOut:
        return "Focus-OUT";
    case KeyAnimation::Pushed:
        return "Pushed";
    case KeyAnimation::ToggleOn:
        return "toggle-ON";
    case KeyAnimation::ToggleOff:
        return "toggle-OFF";
    }
    return "normal";
}

std::u16string OneCharacter(char16_t character)
{
    return std::u16string(1, character);
}

}

KeyboardView::KeyboardView(Display& menuDisplay, const MenuFiles& files, const Text::MessageCatalog& menuMessages)
    : display(menuDisplay)
    , messages(menuMessages)
    , archive(files.OpenLayoutBundle(BundleName))
{
    background = std::make_unique<MenuLayout>(display, *archive, "fs_VK_bg_a");
    field = std::make_unique<MenuLayout>(display, *archive, "fs_VK_textBox_a");
    keys = std::make_unique<MenuLayout>(display, *archive, "fs_VK_ascii_keytop_a");
    bar = std::make_unique<MenuLayout>(display, *archive, "fs_VK_toolbar_a");
    symbols = std::make_unique<MenuLayout>(display, *archive, "fs_signWindow_a");

    fieldButtons = std::make_unique<ButtonController>(*field, display);
    keyButtons = std::make_unique<ButtonController>(*keys, display);
    barButtons = std::make_unique<ButtonController>(*bar, display);
    symbolButtons = std::make_unique<ButtonController>(*symbols, display);

    HideWhatWeeOSDoesNotNeed();
    SetFixedTexts();
    RegisterKeys();

    symbolWindowAppearSlot = symbols->BindAnimationToPane("fs_signWindow_a_SGN_FADE-IN", RootPaneName, Layout::PlaybackMode::PlayOnce);
    symbolWindowDisappearSlot = symbols->BindAnimationToPane("fs_signWindow_a_Scroll_FADE-OUT", RootPaneName, Layout::PlaybackMode::PlayOnce);
}

// The layouts hold the keys of the other languages and the button of the prediction. WeeOS is English and has no prediction
// (docs/audit/OpenPoints/SystemMenu.md SYS-10). The keys of Shift and Caps show a picture, and not the words that the layout holds.
void KeyboardView::HideWhatWeeOSDoesNotNeed()
{
    for (std::string_view paneName : {"N_KeyChange_JP", "N_VK_grid", "N_modeSelect_all", "N_modeSelect_kr", "W_USEU_prdc_lang", "P_key_HENKAN",
             "T_key_SHIFT", "T_key_CAPS"})
        keys->PaneNamed(paneName).isVisible = false;

    bar->PaneNamed("N_keyboardChange").isVisible = false;
}

void KeyboardView::SetFixedTexts()
{
    bar->SetText("T_BT_cancel", messages.Find(CancelMessage));
    bar->SetText("T_BT_confirm", messages.Find(OkMessage));
    keys->SetText("T_USEU_Chng_sign", messages.Find(OthersMessage));
    symbols->SetText("T_SGNkey_close", messages.Find(CloseMessage));
    symbols->SetText("T_SGNkey_prev", messages.Find(BackMessage));
    symbols->SetText("T_SGNkey_next", messages.Find(NextMessage));
}

void KeyboardView::RegisterKeys()
{
    const std::string keyPrefix = "fs_VK_ascii_keytop_a_";
    for (int number = 0; number < static_cast<int>(CharacterKeyCount); ++number)
        RegisterKey({KeyKind::Character, number}, *keys, *keyButtons, std::format("B_key_{:02}", number), std::format("P_key_{:02}", number), keyPrefix);

    RegisterKey({KeyKind::Delete}, *keys, *keyButtons, "B_key_DELETE", "P_key_DELETE", keyPrefix);
    RegisterKey({KeyKind::Enter}, *keys, *keyButtons, "B_key_LF", "P_key_LF", keyPrefix);
    RegisterKey({KeyKind::Shift}, *keys, *keyButtons, "B_key_SHIFT", "P_key_SHIFT", keyPrefix);
    RegisterKey({KeyKind::CapsLock}, *keys, *keyButtons, "B_key_CAPS", "P_key_CAPS", keyPrefix);
    RegisterKey({KeyKind::Space}, *keys, *keyButtons, "B_key_SPACE", "P_key_SPACE", keyPrefix);
    RegisterKey({KeyKind::Symbols}, *keys, *keyButtons, "B_USEU_Chng_sign", "W_USEU_Chng_sign", keyPrefix);

    const std::string barPrefix = "fs_VK_toolbar_a_";
    RegisterKey({KeyKind::Cancel}, *bar, *barButtons, "B_BT_cancel", "P_BT_cancel", barPrefix);
    RegisterKey({KeyKind::Confirm}, *bar, *barButtons, "B_BT_confirm", "P_BT_confirm", barPrefix);

    const std::string symbolKeyPrefix = "fs_signWindow_a_SGN_";
    for (int number = 0; number < SymbolsPerPage; ++number)
        RegisterKey({KeyKind::Symbol, number}, *symbols, *symbolButtons, std::format("B_SGNkey_{:02}", number),
            std::format("P_SGNkey_{:02}", number), symbolKeyPrefix);

    const std::string symbolButtonPrefix = "fs_signWindow_a_";
    RegisterKey({KeyKind::CloseSymbols}, *symbols, *symbolButtons, "B_SGNkey_close", "P_SGNkey_close", symbolButtonPrefix);
    RegisterKey({KeyKind::PreviousSymbolPage}, *symbols, *symbolButtons, "B_SGNkey_prev", "P_SGNkey_prev", symbolButtonPrefix);
    RegisterKey({KeyKind::NextSymbolPage}, *symbols, *symbolButtons, "B_SGNkey_next", "P_SGNkey_next", symbolButtonPrefix);

    // The arrows next to the text field have no animation of their own.
    RegisterKey({KeyKind::ScrollUp}, *field, *fieldButtons, "B_txtScrll_UP", "P_txtScrll_UP", "");
    RegisterKey({KeyKind::ScrollDown}, *field, *fieldButtons, "B_txtScrll_DOWN", "P_txtScrll_DOWN", "");
}

void KeyboardView::RegisterKey(KeyId id, MenuLayout& layout, ButtonController& buttons, const std::string& buttonPane,
    const std::string& picturePane, const std::string& animationPrefix)
{
    buttons.AddButton(buttonPane);
    recordOfButtonPane[buttonPane] = records.size();
    records.push_back({id, &layout, &buttons, buttonPane, picturePane, animationPrefix});
}

KeyboardView::KeyRecord& KeyboardView::RecordOf(KeyId key)
{
    for (KeyRecord& record : records)
    {
        if (record.id == key)
            return record;
    }
    throw MenuError("The soft keyboard has no such key");
}

void KeyboardView::SetOpacity(std::uint8_t opacity)
{
    for (MenuLayout* layout : {background.get(), field.get(), keys.get(), bar.get(), symbols.get()})
    {
        Formats::Pane& rootPane = layout->PaneNamed(RootPaneName);
        rootPane.alpha = opacity;
        rootPane.inheritsAlpha = true;
    }
    isFullyShown = opacity == FullAlpha;
}

void KeyboardView::EnableButtons(bool shouldBeEnabled)
{
    areButtonsEnabled = shouldBeEnabled;
    EnableButtonsThatCanBePressed();
}

void KeyboardView::OpenSymbolWindow(bool shouldBeOpen)
{
    if (shouldBeOpen != isSymbolWindowOpen)
        symbols->StartAnimation(shouldBeOpen ? symbolWindowAppearSlot : symbolWindowDisappearSlot);

    isSymbolWindowOpen = shouldBeOpen;
    EnableButtonsThatCanBePressed();
    keyButtons->ForgetPointers();
    symbolButtons->ForgetPointers();
}

bool KeyboardView::IsSymbolWindowOpen() const
{
    return isSymbolWindowOpen;
}

void KeyboardView::EnableButtonsThatCanBePressed()
{
    for (const KeyRecord& record : records)
        record.buttons->SetEnabled(record.buttonPane, CanBePressed(record));
}

bool KeyboardView::CanBePressed(const KeyRecord& record) const
{
    if (!areButtonsEnabled || !record.isAllowed)
        return false;

    const KeyId& key = record.id;
    switch (key.kind)
    {
    case KeyKind::Character:
    case KeyKind::Delete:
    case KeyKind::Enter:
    case KeyKind::Shift:
    case KeyKind::CapsLock:
    case KeyKind::Space:
    case KeyKind::Symbols:
    case KeyKind::Cancel:
    case KeyKind::Confirm:
        return !isSymbolWindowOpen;
    case KeyKind::Symbol:
        return isSymbolWindowOpen && key.number < symbolCountOfPage;
    case KeyKind::CloseSymbols:
        return isSymbolWindowOpen;
    case KeyKind::PreviousSymbolPage:
    case KeyKind::NextSymbolPage:
        return isSymbolWindowOpen && symbolPageCount > 1;
    case KeyKind::ScrollUp:
        return firstVisibleLine > 0;
    case KeyKind::ScrollDown:
        return firstVisibleLine + VisibleLineCount < lineCount;
    }
    return false;
}

void KeyboardView::ShowCharacters(bool isShiftOn, bool isCapsLockOn)
{
    for (int number = 0; number < static_cast<int>(CharacterKeyCount); ++number)
        keys->SetText(std::format("T_key_{:02}", number), OneCharacter(CharacterOfGridKey(number, isShiftOn, isCapsLockOn)));
}

void KeyboardView::ShowKeyAllowed(KeyId key, bool isAllowed)
{
    KeyRecord& record = RecordOf(key);
    record.isAllowed = isAllowed;

    // The symbol window lies over the grid, so a symbol key that is dimmed as a whole would let the grid show through. It dims only its character.
    std::string dimmedPane = record.picturePane;
    if (key.kind == KeyKind::Symbol)
        dimmedPane = std::format("T_SGNkey_{:02}", key.number);

    Formats::Pane& dimmed = record.layout->PaneNamed(dimmedPane);
    dimmed.alpha = isAllowed ? FullAlpha : NotAllowedKeyAlpha;
    dimmed.inheritsAlpha = true;
    EnableButtonsThatCanBePressed();
}

void KeyboardView::ShowSymbolPage(const std::u16string& pageSymbols, int pageNumber, int pageCount)
{
    symbolCountOfPage = static_cast<int>(pageSymbols.size());
    symbolPageCount = pageCount;

    // The buttons that change the page show only when there is more than one page.
    std::uint8_t pageButtonAlpha = pageCount > 1 ? FullAlpha : 0;
    symbols->PaneNamed("P_SGNkey_prev").alpha = pageButtonAlpha;
    symbols->PaneNamed("P_SGNkey_next").alpha = pageButtonAlpha;

    for (int number = 0; number < SymbolsPerPage; ++number)
    {
        bool hasSymbol = number < symbolCountOfPage;
        symbols->PaneNamed(std::format("P_SGNkey_{:02}", number)).isVisible = hasSymbol;
        symbols->SetText(std::format("T_SGNkey_{:02}", number), hasSymbol ? OneCharacter(pageSymbols[number]) : std::u16string());
    }

    std::string pageText = std::format("{}/{}", pageNumber, pageCount);
    symbols->SetText("T_SGN_pageNumber", std::u16string(pageText.begin(), pageText.end()));
    EnableButtonsThatCanBePressed();
}

void KeyboardView::PlayKeyAnimation(KeyId key, KeyAnimation animation)
{
    KeyRecord& record = RecordOf(key);
    if (record.animationPrefix.empty())
        return;

    StartPictureAnimation(*record.layout, record.picturePane, record.animationPrefix + AnimationEnding(animation));
}

// An animation file moves a whole set of panes. It is bound to the one picture, and what it does to the others is left out.
// A picture plays one animation at a time: the one that played before stops where it is.
void KeyboardView::StartPictureAnimation(MenuLayout& layout, const std::string& picturePane, const std::string& animationName)
{
    std::pair<std::string, std::string> identity = {picturePane, animationName};
    if (!slotOfAnimation.contains(identity))
        slotOfAnimation[identity] = layout.BindAnimationToPane(animationName, picturePane, Layout::PlaybackMode::PlayOnce);

    std::size_t slot = slotOfAnimation[identity];
    auto playing = playingSlotOfPicture.find(picturePane);
    if (playing != playingSlotOfPicture.end() && playing->second != slot)
        layout.StopAnimation(playing->second);

    playingSlotOfPicture[picturePane] = slot;
    layout.StartAnimation(slot);
}

std::vector<KeyEvent> KeyboardView::Poll(const InputManager& input)
{
    std::vector<KeyEvent> events;
    for (ButtonController* buttons : {fieldButtons.get(), keyButtons.get(), barButtons.get(), symbolButtons.get()})
    {
        for (const ButtonEvent& event : buttons->Poll(input))
            events.push_back({event.kind, records[recordOfButtonPane.at(event.buttonName)].id, event.playerIndex});
    }
    return events;
}

void KeyboardView::Update()
{
    for (MenuLayout* layout : {background.get(), field.get(), keys.get(), bar.get(), symbols.get()})
        layout->Calculate();
}

void KeyboardView::Draw()
{
    background->Draw();
    bar->Draw();
    keys->Draw();
    field->Draw();
    DrawFieldMarks();

    if (isSymbolWindowOpen || symbols->IsAnimationPlaying(symbolWindowDisappearSlot))
        symbols->Draw();
}

// ---- The text field

void KeyboardView::ShowField(const FieldLook& look)
{
    field->SetText(TitlePane, look.text.empty() ? look.title : std::u16string());
    field->SetText(FieldTextPane, look.text);
    Text::LaidOutText fullText = field->LayOutText(FieldTextPane);
    lineCount = std::max(1, static_cast<int>(fullText.lines.size()));

    bool hasFieldChanged = look.text != shownText || look.cursor != shownCursor;
    if (hasFieldChanged)
        ScrollToShowCursor(fullText, look.cursor);

    shownText = look.text;
    shownCursor = look.cursor;
    firstVisibleLine = std::clamp(firstVisibleLine, 0, std::max(0, lineCount - VisibleLineCount));

    ShowVisibleLines(look, fullText);
    isCaretShown = look.isCaretShown;

    ShowScrollArrow("P_txtScrll_UP", firstVisibleLine > 0, isScrollUpShown);
    ShowScrollArrow("P_txtScrll_DOWN", firstVisibleLine + VisibleLineCount < lineCount, isScrollDownShown);
    EnableButtonsThatCanBePressed();
}

// The arrow next to the text field fades in when there are lines to scroll to, and fades out when there are none.
void KeyboardView::ShowScrollArrow(const std::string& picturePane, bool shouldBeShown, bool& isShown)
{
    if (shouldBeShown == isShown)
        return;

    isShown = shouldBeShown;
    StartPictureAnimation(*field, picturePane, shouldBeShown ? "fs_VK_textBox_a_Fade_IN" : "fs_VK_textBox_a_Fade_OUT");
}

void KeyboardView::ScrollField(int lineDistance)
{
    firstVisibleLine = std::clamp(firstVisibleLine + lineDistance, 0, std::max(0, lineCount - VisibleLineCount));
}

// The line that holds a character is the last line that starts at or before it. The place behind the last character is on the last line.
int KeyboardView::LineOfCharacter(const Text::LaidOutText& text, std::size_t characterNumber) const
{
    int line = 0;
    for (std::size_t index = 0; index < text.lines.size(); ++index)
    {
        if (text.lines[index].firstCharacter <= characterNumber)
            line = static_cast<int>(index);
    }
    return line;
}

void KeyboardView::ScrollToShowCursor(const Text::LaidOutText& text, std::size_t cursor)
{
    int cursorLine = LineOfCharacter(text, cursor);
    if (cursorLine < firstVisibleLine)
        firstVisibleLine = cursorLine;

    if (cursorLine >= firstVisibleLine + VisibleLineCount)
        firstVisibleLine = cursorLine - VisibleLineCount + 1;
}

// The text box shows the visible lines only. The caret and the selection are found in the lines as the box lays them out.
void KeyboardView::ShowVisibleLines(const FieldLook& look, const Text::LaidOutText& fullText)
{
    std::size_t firstCharacter = 0;
    std::size_t endCharacter = 0;
    if (!fullText.lines.empty())
    {
        int lastVisibleLine = std::min(firstVisibleLine + VisibleLineCount, lineCount) - 1;
        firstCharacter = fullText.lines[firstVisibleLine].firstCharacter;
        endCharacter = fullText.lines[lastVisibleLine].endCharacter;
    }

    field->SetText(FieldTextPane, look.text.substr(firstCharacter, endCharacter - firstCharacter));
    Text::LaidOutText visibleText = field->LayOutText(FieldTextPane);

    caretArea.reset();
    selectionAreas.clear();

    if (look.cursor >= firstCharacter && look.cursor <= endCharacter)
    {
        std::vector<Rectangle> areas = AreasOfCharactersInPicture(visibleText, look.cursor - firstCharacter, look.cursor - firstCharacter);
        if (!areas.empty())
            caretArea = areas.front();
    }

    std::size_t selectionFirst = std::max(look.selectionStart, firstCharacter);
    std::size_t selectionEnd = std::min(look.selectionEnd, endCharacter);
    if (selectionFirst < selectionEnd)
        selectionAreas = AreasOfCharactersInPicture(visibleText, selectionFirst - firstCharacter, selectionEnd - firstCharacter);
}

std::vector<KeyboardView::Rectangle> KeyboardView::AreasOfCharactersInPicture(const Text::LaidOutText& text, std::size_t first, std::size_t end)
{
    std::vector<Rectangle> areas;
    for (const Text::TextArea& area : Text::AreasOfCharacters(text, first, end))
    {
        std::optional<Formats::Vector2> upperLeft = field->PointOfPaneInPicture(FieldTextPane, {area.left, area.top});
        std::optional<Formats::Vector2> lowerRight = field->PointOfPaneInPicture(FieldTextPane, {area.left + area.width, area.top - area.height});
        if (!upperLeft || !lowerRight)
            continue;

        Formats::Vector2 lowerLeft = {std::min(upperLeft->x, lowerRight->x), std::min(upperLeft->y, lowerRight->y)};
        Formats::Vector2 upperRight = {std::max(upperLeft->x, lowerRight->x), std::max(upperLeft->y, lowerRight->y)};
        areas.push_back({lowerLeft, upperRight});
    }
    return areas;
}

void KeyboardView::DrawFieldMarks()
{
    if (!isFullyShown)
        return;

    for (const Rectangle& area : selectionAreas)
        DrawRectangle(area, SelectionColor);

    if (caretArea && isCaretShown && selectionAreas.empty())
    {
        Formats::Vector2 caretLowerLeft = {caretArea->lowerLeft.x - CaretHalfWidth, caretArea->lowerLeft.y};
        Formats::Vector2 caretUpperRight = {caretArea->lowerLeft.x + CaretHalfWidth, caretArea->upperRight.y};
        DrawRectangle({caretLowerLeft, caretUpperRight}, CaretColor);
    }
}

void KeyboardView::DrawRectangle(const Rectangle& area, Formats::Rgba8Color color)
{
    Formats::Vector2 upperLeft = {area.lowerLeft.x, area.upperRight.y};
    Formats::Vector2 lowerRight = {area.upperRight.x, area.lowerLeft.y};

    Graphics::Quad quad = {};
    quad.positions = {{upperLeft, area.upperRight, lowerRight, area.lowerLeft}};
    quad.colors = {{color, color, color, color}};
    display.Device().DrawQuad(quad, Graphics::Material());
}

// The place behind the last character of a line is the end of the line.
float KeyboardView::HorizontalPlaceOfCursor(const Text::LaidOutText& text, const Text::PlacedLine& line, std::size_t cursor) const
{
    if (cursor >= line.endCharacter)
        return line.left + line.width;

    return text.characters[cursor].left;
}

std::size_t KeyboardView::NearestCursorPositionInLine(const Text::LaidOutText& text, const Text::PlacedLine& line, float wantedPlace) const
{
    std::size_t nearestPosition = line.firstCharacter;
    float smallestDistance = std::abs(HorizontalPlaceOfCursor(text, line, nearestPosition) - wantedPlace);

    for (std::size_t position = line.firstCharacter; position <= line.endCharacter; ++position)
    {
        float distance = std::abs(HorizontalPlaceOfCursor(text, line, position) - wantedPlace);
        if (distance < smallestDistance)
        {
            smallestDistance = distance;
            nearestPosition = position;
        }
    }
    return nearestPosition;
}

// The cursor moves up or down by lines. It lands in the other line at the character that is nearest to where the cursor is now.
std::size_t KeyboardView::CursorPositionAfterMoving(const std::u16string& text, std::size_t cursor, int lineDistance)
{
    field->SetText(FieldTextPane, text);
    Text::LaidOutText laidOutText = field->LayOutText(FieldTextPane);
    if (laidOutText.lines.empty())
        return cursor;

    int lastLine = static_cast<int>(laidOutText.lines.size()) - 1;
    int cursorLine = LineOfCharacter(laidOutText, cursor);
    int targetLine = std::clamp(cursorLine + lineDistance, 0, lastLine);
    if (targetLine == cursorLine)
        return cursor;

    float wantedPlace = HorizontalPlaceOfCursor(laidOutText, laidOutText.lines[cursorLine], cursor);
    return NearestCursorPositionInLine(laidOutText, laidOutText.lines[targetLine], wantedPlace);
}

}
