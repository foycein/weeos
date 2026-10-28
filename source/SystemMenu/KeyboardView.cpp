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

    isCharacterKeyAllowed.fill(true);
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

const KeyboardView::KeyRecord& KeyboardView::RecordOf(KeyId key) const
{
    for (const KeyRecord& record : records)
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
    bool canScrollUp = firstVisibleLine > 0;
    bool canScrollDown = firstVisibleLine + VisibleLineCount < lineCount;

    for (const KeyRecord& record : records)
    {
        bool canBePressed = areButtonsEnabled;
        switch (record.id.kind)
        {
        case KeyKind::Character:
            canBePressed = canBePressed && !isSymbolWindowOpen && isCharacterKeyAllowed[record.id.number];
            break;
        case KeyKind::Delete:
        case KeyKind::Enter:
        case KeyKind::Shift:
        case KeyKind::CapsLock:
        case KeyKind::Space:
        case KeyKind::Symbols:
            canBePressed = canBePressed && !isSymbolWindowOpen;
            break;
        case KeyKind::Cancel:
        case KeyKind::Confirm:
            break;
        case KeyKind::Symbol:
            canBePressed = canBePressed && isSymbolWindowOpen && record.id.number < symbolCountOfPage;
            break;
        case KeyKind::CloseSymbols:
            canBePressed = canBePressed && isSymbolWindowOpen;
            break;
        case KeyKind::PreviousSymbolPage:
        case KeyKind::NextSymbolPage:
            canBePressed = canBePressed && isSymbolWindowOpen && symbolPageCount > 1;
            break;
        case KeyKind::ScrollUp:
            canBePressed = canBePressed && canScrollUp;
            break;
        case KeyKind::ScrollDown:
            canBePressed = canBePressed && canScrollDown;
            break;
        }
        record.buttons->SetEnabled(record.buttonPane, canBePressed);
    }
}

void KeyboardView::ShowCharacters(bool isShiftOn, bool isCapsLockOn)
{
    for (int number = 0; number < static_cast<int>(CharacterKeyCount); ++number)
        keys->SetText(std::format("T_key_{:02}", number), OneCharacter(CharacterOfGridKey(number, isShiftOn, isCapsLockOn)));
}

void KeyboardView::ShowCharacterKeyAllowed(int characterKeyNumber, bool isAllowed)
{
    isCharacterKeyAllowed.at(characterKeyNumber) = isAllowed;

    Formats::Pane& picture = keys->PaneNamed(std::format("P_key_{:02}", characterKeyNumber));
    picture.alpha = isAllowed ? FullAlpha : NotAllowedKeyAlpha;
    picture.inheritsAlpha = true;
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
    const KeyRecord& record = RecordOf(key);
    if (record.animationPrefix.empty())
        return;

    std::size_t slot = SlotOfKeyAnimation(record, animation);
    auto playing = playingSlotOfPicture.find(record.picturePane);
    if (playing != playingSlotOfPicture.end() && playing->second != slot)
        record.layout->StopAnimation(playing->second);

    playingSlotOfPicture[record.picturePane] = slot;
    record.layout->StartAnimation(slot);
}

std::size_t KeyboardView::SlotOfKeyAnimation(const KeyRecord& record, KeyAnimation animation)
{
    std::pair<std::string, KeyAnimation> identity = {record.picturePane, animation};
    auto known = slotOfAnimation.find(identity);
    if (known != slotOfAnimation.end())
        return known->second;

    std::string animationName = record.animationPrefix + AnimationEnding(animation);
    std::size_t slot = record.layout->BindAnimationToPane(animationName, record.picturePane, Layout::PlaybackMode::PlayOnce);
    slotOfAnimation[identity] = slot;
    return slot;
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

    field->PaneNamed("P_txtScrll_UP").alpha = firstVisibleLine > 0 ? FullAlpha : 0;
    field->PaneNamed("P_txtScrll_DOWN").alpha = firstVisibleLine + VisibleLineCount < lineCount ? FullAlpha : 0;
    EnableButtonsThatCanBePressed();
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

        Formats::Vector2 lowerLeftCorner = {std::min(upperLeft->x, lowerRight->x), std::min(upperLeft->y, lowerRight->y)};
        Formats::Vector2 upperRightCorner = {std::max(upperLeft->x, lowerRight->x), std::max(upperLeft->y, lowerRight->y)};
        areas.push_back({lowerLeftCorner, upperRightCorner});
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
        Rectangle caret = {{caretArea->first.x - CaretHalfWidth, caretArea->first.y}, {caretArea->first.x + CaretHalfWidth, caretArea->second.y}};
        DrawRectangle(caret, CaretColor);
    }
}

void KeyboardView::DrawRectangle(const Rectangle& area, Formats::Rgba8Color color)
{
    Graphics::Quad quad = {};
    quad.positions = {{{area.first.x, area.second.y}, {area.second.x, area.second.y}, {area.second.x, area.first.y}, {area.first.x, area.first.y}}};
    quad.colors = {{color, color, color, color}};
    display.Device().DrawQuad(quad, Graphics::Material());
}

// The cursor moves up or down by lines. It lands in the other line at the character that is nearest to where the cursor is now.
std::size_t KeyboardView::CursorPositionAfterMoving(const std::u16string& text, std::size_t cursor, int lineDistance)
{
    field->SetText(FieldTextPane, text);
    Text::LaidOutText laidOutText = field->LayOutText(FieldTextPane);
    if (laidOutText.lines.empty())
        return cursor;

    auto penPosition = [&](std::size_t position, const Text::PlacedLine& line) {
        if (position >= line.endCharacter)
            return line.left + line.width;

        return laidOutText.characters[position].left;
    };

    int lastLine = static_cast<int>(laidOutText.lines.size()) - 1;
    int cursorLine = LineOfCharacter(laidOutText, cursor);
    int targetLine = std::clamp(cursorLine + lineDistance, 0, lastLine);
    if (targetLine == cursorLine)
        return cursor;

    float wantedPen = penPosition(cursor, laidOutText.lines[cursorLine]);
    const Text::PlacedLine& target = laidOutText.lines[targetLine];

    std::size_t nearestPosition = target.firstCharacter;
    float smallestDistance = std::abs(penPosition(nearestPosition, target) - wantedPen);
    for (std::size_t position = target.firstCharacter; position <= target.endCharacter; ++position)
    {
        float distance = std::abs(penPosition(position, target) - wantedPen);
        if (distance < smallestDistance)
        {
            smallestDistance = distance;
            nearestPosition = position;
        }
    }
    return nearestPosition;
}

}
