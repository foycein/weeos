#include "Channels/MiiChannel/EditorScreen.h"

#include "Channels/MiiChannel/MiiChannelFiles.h"
#include "Channels/MiiChannel/MiiChannelInput.h"

#include "Mii/MiiColors.h"
#include "Mii/MiiCreateId.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <map>
#include <set>

namespace Channels
{

namespace
{

constexpr std::string_view LayoutName = "editNigaoe_00";
constexpr std::string_view MiiPane = "humanNull_00";
constexpr int PlacesPerGridPage = 12;

// The editor keeps the head of its one Mii under this key.
constexpr std::size_t EditedMiiHeadKey = 0;

// The sparkles of a changed part stand this far above the head of the Mii (0x80040BFC, its constants 0x803BE7B8 to 0x803BE7F0): a fixed
// height for the face, the hair and the beard, and for the parts that move up and down a height that falls by 1.8 for each step of
// their place. The eyes count their place from 3.
constexpr float FaceSparkleHeight = 20;
constexpr float HairSparkleHeight = 50;
constexpr float BeardSparkleHeight = 5;
constexpr float SparkleDropPerStep = 1.8f;
constexpr float EyebrowSparkleTop = 50;
constexpr float EyeSparkleTop = 50;
constexpr int EyeSparklePlaceStart = 3;
constexpr float NoseSparkleTop = 37;
constexpr float MouthSparkleTop = 32;
constexpr float MustacheSparkleTop = 29;
constexpr float GlassesSparkleTop = 48;
constexpr float MoleSparkleTop = 50;

constexpr int NoBeardType = 0;

float PlacedSparkleHeight(float top, int place)
{
    return top - SparkleDropPerStep * static_cast<float>(place);
}

float TypeSparkleHeight(const Mii::MiiRecord& record, EditorPart part)
{
    switch (part)
    {
    case EditorPart::FaceShape:
    case EditorPart::MakeUp:
        return FaceSparkleHeight;
    case EditorPart::Hair:
        return HairSparkleHeight;
    case EditorPart::Eyebrows:
        return PlacedSparkleHeight(EyebrowSparkleTop, record.eyebrows.vertical);
    case EditorPart::Eyes:
        return PlacedSparkleHeight(EyeSparkleTop, record.eyes.vertical - EyeSparklePlaceStart);
    case EditorPart::Nose:
        return PlacedSparkleHeight(NoseSparkleTop, record.nose.vertical);
    case EditorPart::Mouth:
        return PlacedSparkleHeight(MouthSparkleTop, record.mouth.vertical);
    case EditorPart::Glasses:
        return PlacedSparkleHeight(GlassesSparkleTop, record.glasses.vertical);
    case EditorPart::Mustache:
        return PlacedSparkleHeight(MustacheSparkleTop, record.facialHair.mustacheVertical);
    case EditorPart::Beard:
        return BeardSparkleHeight;
    case EditorPart::Mole:
        return PlacedSparkleHeight(MoleSparkleTop, record.mole.vertical);
    }
    return FaceSparkleHeight;
}

// The favorite color has no sparkles; the color of the facial hair sparkles at the mustache without a beard, at the beard with one.
std::optional<float> ColorSparkleHeight(const Mii::MiiRecord& record, ColorPart part)
{
    switch (part)
    {
    case ColorPart::Skin:
        return FaceSparkleHeight;
    case ColorPart::Hair:
        return HairSparkleHeight;
    case ColorPart::Eyebrows:
        return TypeSparkleHeight(record, EditorPart::Eyebrows);
    case ColorPart::Eyes:
        return TypeSparkleHeight(record, EditorPart::Eyes);
    case ColorPart::Mouth:
        return TypeSparkleHeight(record, EditorPart::Mouth);
    case ColorPart::Glasses:
        return TypeSparkleHeight(record, EditorPart::Glasses);
    case ColorPart::FacialHair:
        if (record.facialHair.beardType == NoBeardType)
            return TypeSparkleHeight(record, EditorPart::Mustache);
        return BeardSparkleHeight;
    case ColorPart::Favorite:
        return std::nullopt;
    }
    return std::nullopt;
}

// The windows of the nine pages, in the order of the tabs.
constexpr std::array<std::string_view, EditorPageCount> PageWindows = {"windowProfile", "windowBody", "windowFace", "windowHair",
    "windowEyeB", "windowEye", "windowNose", "windowMouth", "windowEtc"};

// A grid of parts: its pages (frmHairPrNull_00, _01, ...), its buttons (frameHair_00, ...) from the first number on, and its arrows.
struct GridPanes
{
    EditorPart part;
    std::string_view pagePrefix;
    int pageCount;
    std::string_view buttonPrefix;
    int firstButton;
    std::string_view arrowPrefix;
};

constexpr std::array<GridPanes, 11> Grids = {{
    {EditorPart::FaceShape, "frmFacePrNull_", 1, "frameFace_", 0, ""},
    {EditorPart::MakeUp, "frmFacePrNull_", 1, "frameFace_", 12, ""},
    {EditorPart::Hair, "frmHairPrNull_", 6, "frameHair_", 0, "H"},
    {EditorPart::Eyebrows, "frmEyeBPrNull_", 2, "frameEyeB_", 0, "EB"},
    {EditorPart::Eyes, "frmEyePrNull_", 4, "frameEye_", 0, "E"},
    {EditorPart::Nose, "frmNosePrNull_", 1, "frameNose_", 0, ""},
    {EditorPart::Mouth, "frmMoutPrNull_", 2, "frameMouth_", 0, "M"},
    {EditorPart::Glasses, "frmEtcPrNull_", 1, "frameEtc_", 0, ""},
    {EditorPart::Mustache, "frmEtcPrNull_", 1, "frameEtc_", 12, ""},
    {EditorPart::Mole, "frmEtcPrNull_", 1, "frameEtc_", 24, ""},
    {EditorPart::Beard, "frmEtcPrNull_", 1, "frameEtc_", 16, ""},
}};

// A palette: its buttons (colorPHair_00, ...) from the first number on, and the colors they show.
struct PalettePanes
{
    ColorPart part;
    std::string_view buttonPrefix;
    int firstButton;
};

constexpr std::array<PalettePanes, 7> Palettes = {{
    {ColorPart::Skin, "colorPFace_", 0},
    {ColorPart::Hair, "colorPHair_", 0},
    {ColorPart::Eyebrows, "colorPEyeB_", 0},
    {ColorPart::Eyes, "colorPEye_", 0},
    {ColorPart::Mouth, "colorPMouth_", 0},
    {ColorPart::Glasses, "colorPEtc_", 8},
    {ColorPart::FacialHair, "colorPEtc_", 0},
}};

// The buttons that size and move a part (editEBFrm_00, ...); the picture on each says what it does.
struct AdjustPanes
{
    EditorPart part;
    std::string_view buttonPrefix;
    int firstButton;
    int buttonCount;
};

constexpr std::array<AdjustPanes, 7> AdjustGroups = {{
    {EditorPart::Eyebrows, "editEBFrm", 0, 8},
    {EditorPart::Eyes, "editEFrm", 0, 8},
    {EditorPart::Nose, "editNsFrm", 0, 4},
    {EditorPart::Mouth, "editMFrm", 0, 4},
    {EditorPart::Glasses, "editEtFrm", 0, 4},
    {EditorPart::Mustache, "editEtFrm", 4, 4},
    {EditorPart::Mole, "editEtFrm", 8, 6},
}};

const std::map<std::string, Adjustment> AdjustmentOfPicture = {
    {"sk_editEnlargement_00.tpl", Adjustment::Larger},
    {"sk_editReduction_00.tpl", Adjustment::Smaller},
    {"sk_editRotL_00.tpl", Adjustment::TurnLeft},
    {"sk_editRotR_00.tpl", Adjustment::TurnRight},
    {"sk_editNear_00.tpl", Adjustment::Closer},
    {"sk_editWide_00.tpl", Adjustment::Farther},
    {"sk_editUp_00.tpl", Adjustment::Up},
    {"sk_editDown_00.tpl", Adjustment::Down},
    {"sk_editleft_00.tpl", Adjustment::Left},
    {"sk_editRight_00.tpl", Adjustment::Right},
};

// The small tabs of the face page (shape, make-up) and of the features page (glasses, mustache, mole, beard).
constexpr std::array<EditorPart, 2> FaceSubParts = {EditorPart::FaceShape, EditorPart::MakeUp};
constexpr std::array<EditorPart, 4> FeatureSubParts = {EditorPart::Glasses, EditorPart::Mustache, EditorPart::Mole, EditorPart::Beard};
constexpr std::array<std::string_view, 4> FeatureAdjustNulls = {"editEtFrmPrN_00", "editEtFrmPrN_01", "editEtFrmPrN_02", ""};
constexpr std::string_view GlassesPalette = "cpEtcPrNull_01";
constexpr std::string_view FacialHairPalette = "cpEtcPrNull_00";

constexpr std::string_view HairFlipButton = "editHairFrm_02";

// The body page: the knobs of height and weight on their bars, 280 units long.
constexpr std::array<std::string_view, 2> SliderBars = {"barBody_00", "barBody_01"};
constexpr std::array<std::string_view, 2> SliderKnobs = {"btnBodyNull_00", "btnBodyNull_01"};
constexpr int HeightSlider = 0;
constexpr float SliderLength = 280.0f;
constexpr int SliderMaximum = 128;

// The profile page's rows.
constexpr std::string_view BirthdayButton = "frmProfile_01";
constexpr std::string_view GenderButton = "frmProfile_02";
constexpr std::string_view ColorButtonPane = "frmProfile_03";
constexpr std::string_view CreatorButton = "frmProfile_04";
constexpr std::string_view MingleButton = "frmProfile_06";
constexpr std::string_view SpecialMingleRow = "frmProfile_07";
constexpr std::string_view MingleRow = "frmProfNull_06";
constexpr std::string_view NicknameText = "textProf_00";
constexpr std::string_view BirthdayText = "textProf_01";
constexpr std::string_view GenderText = "textProf_02";
constexpr std::string_view CreatorText = "textProf_04";
constexpr std::string_view FavoriteMark = "PicProf_05";
constexpr std::string_view MingleText = "textProf_06";
constexpr std::uint32_t MaleWord = 300101;
constexpr std::uint32_t FemaleWord = 300201;
constexpr std::uint32_t MingleOffWord = 200801;
constexpr std::uint32_t MingleOnWord = 200802;
constexpr std::uint32_t EnterNickname = 300001;
constexpr std::uint32_t EnterCreatorName = 300007;

// Assumed (CHN-67): the original marks the chosen part by colors of its button that were not traced; WeeOS tints the chosen button.
constexpr Formats::Rgba8Color ChosenTint = {255, 210, 90, 255};
constexpr Formats::Rgba8Color NotChosenTint = {255, 255, 255, 255};
constexpr std::uint8_t ChosenColorAlpha = 255;
constexpr std::uint8_t OtherColorAlpha = 200;

// The sounds of the editor (the strings of 8003CBDC to 8003F26C).
constexpr std::string_view TabSound = "SEQ_EDIT_BUT_PUSH_TOPMENU";
constexpr std::string_view PartSound = "SEQ_EDIT_STAMP_PART";
constexpr std::string_view ColorSound = "SEQ_EDIT_STAMP_COLOR";
constexpr std::string_view LargerSound = "SEQ_EDIT_PARTS_UP";
constexpr std::string_view SmallerSound = "SEQ_EDIT_PARTS_DOWN";
constexpr std::string_view UpperLimitSound = "SEQ_EDIT_PARTS_UP_LIMIT";
constexpr std::string_view LowerLimitSound = "SEQ_EDIT_PARTS_DOWN_LIMIT";
constexpr std::string_view PageUpSound = "SEQ_EDIT_BUT_PUSH_PARTS_TYPE_PAGE_UP";
constexpr std::string_view PageDownSound = "SEQ_EDIT_BUT_PUSH_PARTS_TYPE_PAGE_DOWN";
constexpr std::string_view FlipSound = "SEQ_EDIT_BUT_PUSH_PARTS_HAIR_REVERSE";
constexpr std::string_view BodyUpSound = "SEQ_EDIT_BODY_UP";
constexpr std::string_view BodyDownSound = "SEQ_EDIT_BODY_DOWN";
constexpr std::string_view QuitSound = "SEQ_EDIT_BUT_PUSH_EXIT";
constexpr std::string_view SaveSound = "SEQ_EDITOR_EXIT_SAVE";
constexpr std::string_view NoSaveSound = "SEQ_EDITOR_EXIT_NOSAVE";
constexpr std::string_view ButtonSound = "SEQ_EDIT_BUT_PUSH";
constexpr std::string_view FavoriteOnSound = "SEQ_FAVORITE_ON";
constexpr std::string_view FavoriteOffSound = "SEQ_FAVORITE_OFF";

const GridPanes& GridOf(EditorPart part)
{
    for (const GridPanes& grid : Grids)
    {
        if (grid.part == part)
            return grid;
    }
    return Grids.front();
}

const PalettePanes& PaletteOf(ColorPart part)
{
    for (const PalettePanes& palette : Palettes)
    {
        if (palette.part == part)
            return palette;
    }
    return Palettes.front();
}

const AdjustPanes* AdjustGroupOf(EditorPart part)
{
    for (const AdjustPanes& group : AdjustGroups)
    {
        if (group.part == part)
            return &group;
    }
    return nullptr;
}

Formats::Rgba8Color PaletteColor(ColorPart part, int color)
{
    switch (part)
    {
    case ColorPart::Skin:
        return Mii::SkinColors[color];
    case ColorPart::Hair:
    case ColorPart::Eyebrows:
    case ColorPart::FacialHair:
        return Mii::HairColors[color];
    case ColorPart::Eyes:
        return Mii::EyeColors[color];
    case ColorPart::Mouth:
        return Mii::LipColors[color];
    case ColorPart::Glasses:
        return Mii::GlassesColors[color];
    case ColorPart::Favorite:
        return Mii::FavoriteColors[color];
    }
    return {};
}

void TintWindow(Formats::Pane& pane, Formats::Rgba8Color tint)
{
    if (pane.window)
        pane.window->content.vertexColors.fill(tint);
}

std::u16string Ascii(std::string_view text)
{
    return std::u16string(text.begin(), text.end());
}

}

EditorScreen::EditorScreen(ChannelServices& channelServices, const Layout::LayoutArchive& layouts,
    const Formats::NumberedMessageFile& messages, const MiiFigures& miiFigures, const Mii::MiiResources& faceParts,
    MessageWindow& messageWindow, ProfileDialog& profileDialog, SystemMenu::SoftKeyboard& softKeyboard, MiiChannelData& channelData,
    MiiEffects& miiEffects)
    : services(channelServices)
    , texts(messages)
    , figures(miiFigures)
    , effects(miiEffects)
    , message(messageWindow)
    , profile(profileDialog)
    , keyboard(softKeyboard)
    , data(channelData)
    , layout(std::make_unique<SystemMenu::MenuLayout>(channelServices.display, layouts, std::string(LayoutName)))
    , heads(channelServices.display.Device(), faceParts)
{
    FillTextsFromUserData(*layout, messages);

    for (const PalettePanes& palette : Palettes)
    {
        for (int color = 0; color < ColorCount(palette.part); ++color)
            TintWindow(layout->PaneNamed(ColorButton(palette.part, color)), PaletteColor(palette.part, color));
    }
}

void EditorScreen::Open(const Mii::MiiRecord& record, std::optional<std::size_t> slot)
{
    editor.emplace(record);
    editedSlot = slot;
    result.reset();
    pending = Pending::Nothing;
    gridPages.fill(0);
    for (const GridPanes& grid : Grids)
    {
        int chosenPlace = GridPlaceOfType(grid.part, editor->ChosenType(grid.part));
        gridPages[static_cast<std::size_t>(grid.part)] = chosenPlace / PlacesPerGridPage;
    }
    ShowPage(EditorPage::Face);
}

bool EditorScreen::IsOpen() const
{
    return editor.has_value();
}

std::optional<EditorResult> EditorScreen::TakeResult()
{
    std::optional<EditorResult> taken = result;
    result.reset();
    return taken;
}

const MiiEditor& EditorScreen::Editor() const
{
    return *editor;
}

EditorPage EditorScreen::Page() const
{
    return page;
}

std::string EditorScreen::TabButton(EditorPage tabPage)
{
    return std::format("mainMFrm_{:02}", static_cast<int>(tabPage));
}

std::string EditorScreen::GridButton(EditorPart part, int place) const
{
    const GridPanes& grid = GridOf(part);
    return std::format("{}{:02}", grid.buttonPrefix, grid.firstButton + place);
}

std::string EditorScreen::ColorButton(ColorPart part, int color) const
{
    const PalettePanes& palette = PaletteOf(part);
    return std::format("{}{:02}", palette.buttonPrefix, palette.firstButton + color);
}

std::string EditorScreen::AdjustButton(EditorPart part, Adjustment adjustment) const
{
    const AdjustPanes* group = AdjustGroupOf(part);
    if (group == nullptr)
        return {};

    for (int button = group->firstButton; button < group->firstButton + group->buttonCount; ++button)
    {
        const Formats::Pane& icon = layout->PaneNamed(std::format("{}Icon_{:02}", group->buttonPrefix, button));
        const std::string& picture = layout->MaterialOf(icon).textureNames.front();
        auto known = AdjustmentOfPicture.find(picture);
        if (known != AdjustmentOfPicture.end() && known->second == adjustment)
            return std::format("{}_{:02}", group->buttonPrefix, button);
    }
    return {};
}

Formats::Vector2 EditorScreen::PlaceOf(std::string_view paneName) const
{
    return layout->PointOfPaneInPicture(paneName, {0, 0}).value_or(Formats::Vector2{0, 0});
}

void EditorScreen::Update(bool acceptsInput)
{
    layout->Calculate();
    if (!editor)
        return;

    waitFrame = std::fmod(waitFrame + 1, AnimationFramesOf(PlazaActivity::Waiting));
    heads.Prepare({{EditedMiiHeadKey, editor->Record()}});

    if (keyboard.IsOpen())
    {
        keyboard.Update();
        HandleKeyboardAnswer();
        return;
    }

    HandleDialogAnswers();
    HandleMessageAnswer();
    bool isBusy = !editor || pending != Pending::Nothing || message.IsOpen() || profile.IsOpen() || keyboard.IsOpen();
    if (!acceptsInput || isBusy)
        return;

    if (IsPressed(QuitButton))
    {
        AskQuit();
        return;
    }

    for (int tab = 0; tab < EditorPageCount; ++tab)
    {
        if (IsPressed(TabButton(static_cast<EditorPage>(tab))))
        {
            services.sounds.Request(TabSound);
            ShowPage(static_cast<EditorPage>(tab));
            return;
        }
    }

    HandlePageButtons();
}

void EditorScreen::Draw()
{
    if (!editor)
        return;

    layout->Draw();
    const Mii::MiiHead* head = heads.Find(EditedMiiHeadKey);
    if (head != nullptr)
    {
        PictureRectangle miiRectangle = RectangleOfPane(*layout, MiiPane);
        figures.DrawStandingMii(editor->Record(), *head, miiRectangle, services.display.GetCanvas(), waitFrame);
        FigureCamera camera = figures.StandingCamera(miiRectangle, services.display.GetCanvas());
        effects.DrawAmongModels(camera.view, camera.projection);
    }

    if (keyboard.IsOpen())
        keyboard.Draw();
}

void EditorScreen::ShowPage(EditorPage newPage)
{
    page = newPage;
    for (int window = 0; window < EditorPageCount; ++window)
        layout->PaneNamed(PageWindows[window]).isVisible = window == static_cast<int>(page);

    ShowSubPart(0);
}

void EditorScreen::ShowSubPart(int newSubPart)
{
    subPart = newSubPart;

    if (page == EditorPage::Face)
    {
        for (std::size_t part = 0; part < FaceSubParts.size(); ++part)
            layout->PaneNamed(std::format("frmFacePrNull_{:02}", part)).isVisible = static_cast<int>(part) == subPart;
    }

    if (page == EditorPage::Features)
    {
        for (std::size_t part = 0; part < FeatureSubParts.size(); ++part)
        {
            bool isShown = static_cast<int>(part) == subPart;
            layout->PaneNamed(std::format("frmEtcPrNull_{:02}", part)).isVisible = isShown;
            if (!FeatureAdjustNulls[part].empty())
                layout->PaneNamed(FeatureAdjustNulls[part]).isVisible = isShown;
        }
        EditorPart shown = ShownPart();
        layout->PaneNamed(GlassesPalette).isVisible = shown == EditorPart::Glasses;
        layout->PaneNamed(FacialHairPalette).isVisible = shown == EditorPart::Mustache || shown == EditorPart::Beard;
    }

    RefreshSelections();
}

std::vector<EditorPart> EditorScreen::PartsOfPage(EditorPage pageOfParts) const
{
    switch (pageOfParts)
    {
    case EditorPage::Face:
        return {FaceSubParts[subPart]};
    case EditorPage::Hair:
        return {EditorPart::Hair};
    case EditorPage::Eyebrows:
        return {EditorPart::Eyebrows};
    case EditorPage::Eyes:
        return {EditorPart::Eyes};
    case EditorPage::Nose:
        return {EditorPart::Nose};
    case EditorPage::Mouth:
        return {EditorPart::Mouth};
    case EditorPage::Features:
        return {FeatureSubParts[subPart]};
    default:
        return {};
    }
}

EditorPart EditorScreen::ShownPart() const
{
    std::vector<EditorPart> parts = PartsOfPage(page);
    if (parts.empty())
        return EditorPart::FaceShape;
    return parts.front();
}

void EditorScreen::RefreshSelections()
{
    for (const GridPanes& grid : Grids)
        RefreshGridPage(grid.part);

    for (const PalettePanes& palette : Palettes)
    {
        for (int color = 0; color < ColorCount(palette.part); ++color)
        {
            Formats::Pane& button = layout->PaneNamed(ColorButton(palette.part, color));
            bool isChosen = color == editor->ChosenColor(palette.part);
            button.alpha = isChosen ? ChosenColorAlpha : OtherColorAlpha;
        }
    }

    RefreshProfile();
    RefreshBodySliders();
}

void EditorScreen::RefreshGridPage(EditorPart part)
{
    const GridPanes& grid = GridOf(part);
    int chosenPlace = GridPlaceOfType(part, editor->ChosenType(part));
    for (int place = 0; place < TypeCount(part); ++place)
        TintWindow(layout->PaneNamed(GridButton(part, place)), place == chosenPlace ? ChosenTint : NotChosenTint);

    if (grid.pageCount <= 1)
        return;

    int shownPage = gridPages[static_cast<std::size_t>(part)];
    for (int gridPage = 0; gridPage < grid.pageCount; ++gridPage)
        layout->PaneNamed(std::format("{}{:02}", grid.pagePrefix, gridPage)).isVisible = gridPage == shownPage;

    layout->SetText(std::format("{}TextNumber_00", grid.arrowPrefix), Ascii(std::format("{}/{}", shownPage + 1, grid.pageCount)));
}

void EditorScreen::RefreshProfile()
{
    const Mii::MiiRecord& record = editor->Record();
    layout->SetText(NicknameText, record.name);

    std::string birthday = "--/--";
    if (record.birthMonth != 0)
        birthday = std::format("{}/{}", record.birthMonth, record.birthDay);
    layout->SetText(BirthdayText, Ascii(birthday));

    std::uint32_t genderWord = record.gender == Mii::Gender::Female ? FemaleWord : MaleWord;
    layout->SetText(GenderText, MessageText(texts, genderWord));
    TintWindow(layout->PaneNamed(ColorButtonPane), Mii::FavoriteColors[record.favoriteColor]);
    layout->SetText(CreatorText, record.creatorName);
    layout->PaneNamed(FavoriteMark).isVisible = record.isFavorite;
    if (record.isFavorite && !isFavoriteMarkShown)
        effects.StartFlat(MiiEffects::FavoriteMarkSparkles, PlaceOf(FavoriteMark));
    isFavoriteMarkShown = record.isFavorite;

    bool isSpecial = Mii::IsSpecialMii(record);
    layout->PaneNamed(SpecialMingleRow).isVisible = isSpecial;
    layout->PaneNamed(MingleRow).isVisible = !isSpecial;
    layout->SetText(MingleText, MessageText(texts, record.mayNotTravel ? MingleOffWord : MingleOnWord));
}

void EditorScreen::RefreshBodySliders()
{
    const Mii::MiiRecord& record = editor->Record();
    std::array<int, 2> values = {record.height, record.weight};
    for (std::size_t slider = 0; slider < SliderKnobs.size(); ++slider)
    {
        float place = (static_cast<float>(values[slider]) / SliderMaximum - 0.5f) * SliderLength;
        layout->PaneNamed(SliderKnobs[slider]).translation.x = layout->PaneNamed(SliderBars[slider]).translation.x + place;
    }
}

bool EditorScreen::HandlePageButtons()
{
    if (page == EditorPage::Profile)
        return HandleProfileRows();

    if (page == EditorPage::Body)
        return HandleBodySliders();

    if (HandleSubTabs())
        return true;

    if (page == EditorPage::Hair && IsPressed(HairFlipButton))
    {
        editor->FlipHair();
        services.sounds.Request(FlipSound);
        return true;
    }

    for (EditorPart part : PartsOfPage(page))
    {
        if (HandleGridArrows(part) || HandleGrid(part) || HandleAdjustButtons(part))
            return true;

        std::optional<ColorPart> colorPart = ColorPartOf(part);
        if (colorPart && HandlePalette(*colorPart))
            return true;
    }
    return false;
}

bool EditorScreen::HandleGrid(EditorPart part)
{
    const GridPanes& grid = GridOf(part);
    int shownPage = gridPages[static_cast<std::size_t>(part)];
    int firstPlace = 0;
    int lastPlace = TypeCount(part);
    if (grid.pageCount > 1)
    {
        firstPlace = shownPage * PlacesPerGridPage;
        lastPlace = std::min(lastPlace, firstPlace + PlacesPerGridPage);
    }

    for (int place = firstPlace; place < lastPlace; ++place)
    {
        if (!IsPressed(GridButton(part, place)))
            continue;

        editor->ChooseType(part, TypeAtGridPlace(part, place));
        SparkleAtThePart(TypeSparkleHeight(editor->Record(), part));
        services.sounds.Request(PartSound);
        RefreshSelections();
        return true;
    }
    return false;
}

bool EditorScreen::HandleGridArrows(EditorPart part)
{
    const GridPanes& grid = GridOf(part);
    if (grid.pageCount <= 1)
        return false;

    int& shownPage = gridPages[static_cast<std::size_t>(part)];
    if (IsPressed(std::format("{}ArrowUp_00", grid.arrowPrefix)))
    {
        shownPage = (shownPage + grid.pageCount - 1) % grid.pageCount;
        services.sounds.Request(PageUpSound);
        RefreshGridPage(part);
        return true;
    }

    if (IsPressed(std::format("{}ArrowDown_00", grid.arrowPrefix)))
    {
        shownPage = (shownPage + 1) % grid.pageCount;
        services.sounds.Request(PageDownSound);
        RefreshGridPage(part);
        return true;
    }
    return false;
}

bool EditorScreen::HandlePalette(ColorPart part)
{
    for (int color = 0; color < ColorCount(part); ++color)
    {
        if (!IsPressed(ColorButton(part, color)))
            continue;

        editor->ChooseColor(part, color);
        if (std::optional<float> height = ColorSparkleHeight(editor->Record(), part))
            SparkleAtThePart(*height);
        services.sounds.Request(ColorSound);
        RefreshSelections();
        return true;
    }
    return false;
}

bool EditorScreen::HandleAdjustButtons(EditorPart part)
{
    const AdjustPanes* group = AdjustGroupOf(part);
    if (group == nullptr)
        return false;

    for (const auto& [picture, adjustment] : AdjustmentOfPicture)
    {
        std::string button = AdjustButton(part, adjustment);
        if (button.empty() || !IsPressed(button))
            continue;

        bool growsOrRises = adjustment == Adjustment::Larger || adjustment == Adjustment::Up || adjustment == Adjustment::TurnLeft
            || adjustment == Adjustment::Farther || adjustment == Adjustment::Right;
        AdjustResult adjusted = editor->Adjust(part, adjustment);
        if (adjusted == AdjustResult::Changed)
            services.sounds.Request(growsOrRises ? LargerSound : SmallerSound);
        else if (adjusted == AdjustResult::AtLimit)
            services.sounds.Request(growsOrRises ? UpperLimitSound : LowerLimitSound);
        return true;
    }
    return false;
}

bool EditorScreen::HandleSubTabs()
{
    if (page == EditorPage::Face)
    {
        for (int part = 0; part < static_cast<int>(FaceSubParts.size()); ++part)
        {
            if (IsPressed(std::format("editFaceFrm_{:02}", part)))
            {
                services.sounds.Request(ButtonSound);
                ShowSubPart(part);
                return true;
            }
        }
    }

    if (page == EditorPage::Features)
    {
        for (int part = 0; part < static_cast<int>(FeatureSubParts.size()); ++part)
        {
            if (IsPressed(std::format("editEtcFrm_{:02}", part)))
            {
                services.sounds.Request(ButtonSound);
                ShowSubPart(part);
                return true;
            }
        }
    }
    return false;
}

// A knob follows the hand while A is held, from the place where A went down on the bar.
bool EditorScreen::HandleBodySliders()
{
    PlayerOneInput input = ReadPlayerOne(services);
    if (!input.IsHeld(Input::RemoteButtons::A))
        draggedSlider.reset();

    if (!input.pointer)
        return false;

    if (!draggedSlider && input.WasPressed(Input::RemoteButtons::A))
    {
        for (std::size_t slider = 0; slider < SliderBars.size(); ++slider)
        {
            if (layout->IsPointInPane(SliderBars[slider], *input.pointer) || layout->IsPointInPane(SliderKnobs[slider], *input.pointer))
                draggedSlider = static_cast<int>(slider);
        }
    }

    if (!draggedSlider)
        return false;

    std::string_view bar = SliderBars[*draggedSlider];
    std::optional<Formats::Vector2> leftEnd = layout->PointOfPaneInPicture(bar, {-SliderLength / 2, 0});
    std::optional<Formats::Vector2> rightEnd = layout->PointOfPaneInPicture(bar, {SliderLength / 2, 0});
    if (!leftEnd || !rightEnd)
        return false;

    float fraction = (input.pointer->x - leftEnd->x) / (rightEnd->x - leftEnd->x);
    int value = static_cast<int>(std::lround(std::clamp(fraction, 0.0f, 1.0f) * SliderMaximum));
    const Mii::MiiRecord& record = editor->Record();
    bool isHeight = *draggedSlider == HeightSlider;
    int oldValue = isHeight ? record.height : record.weight;
    if (value == oldValue)
        return true;

    if (isHeight)
        editor->SetHeight(value);
    else
        editor->SetWeight(value);

    services.sounds.Request(value > oldValue ? BodyUpSound : BodyDownSound);
    RefreshBodySliders();
    return true;
}

bool EditorScreen::HandleProfileRows()
{
    const Mii::MiiRecord& record = editor->Record();
    if (IsPressed(NicknameButton))
    {
        OpenKeyboard(EnterNickname, record.name, false);
        return true;
    }

    if (IsPressed(BirthdayButton))
    {
        Ask(ProfileQuestion::Month);
        return true;
    }

    if (IsPressed(GenderButton))
    {
        Ask(ProfileQuestion::Gender);
        return true;
    }

    if (IsPressed(ColorButtonPane))
    {
        Ask(ProfileQuestion::FavoriteColor);
        return true;
    }

    if (IsPressed(CreatorButton))
    {
        OfferCreatorNames();
        return true;
    }

    if (IsPressed(FavoriteButton))
    {
        ToggleFavorite();
        return true;
    }

    if (!Mii::IsSpecialMii(record) && IsPressed(MingleButton))
    {
        Ask(ProfileQuestion::Mingle);
        return true;
    }
    return false;
}

// "Choose a creator" offers the creators of the Miis of the plaza, each once.
void EditorScreen::OfferCreatorNames()
{
    std::vector<std::u16string> creators;
    std::set<std::u16string> seen;
    for (const StoredMii& stored : data.Miis())
    {
        const std::u16string& creator = stored.record.creatorName;
        bool isNew = !creator.empty() && seen.insert(creator).second;
        if (isNew)
            creators.push_back(creator);
    }
    creatorChoices = creators;
    profile.SetCreatorNames(creators);
    Ask(ProfileQuestion::Creator);
}

// A new favorite sparkles on the Mii's pants.
void EditorScreen::ToggleFavorite()
{
    const Mii::MiiRecord& record = editor->Record();
    editor->SetFavorite(!record.isFavorite);
    services.sounds.Request(record.isFavorite ? FavoriteOnSound : FavoriteOffSound);
    if (record.isFavorite)
    {
        bool isWoman = record.gender == Mii::Gender::Female;
        effects.Start(isWoman ? MiiEffects::FavoriteSparklesOfAWoman : MiiEffects::FavoriteSparkles, Graphics::Matrix4());
    }
    RefreshProfile();
}

void EditorScreen::OpenKeyboard(std::uint32_t titleNumber, const std::u16string& initialText, bool isForTheCreator)
{
    SystemMenu::KeyboardRequest request;
    request.rules.maximumLength = Mii::MiiNameLength;
    request.title = MessageText(texts, titleNumber);
    request.initialText = initialText;
    isTypingCreator = isForTheCreator;
    keyboard.Open(request);
}

void EditorScreen::Ask(ProfileQuestion question)
{
    constexpr bool OverItsOwnBackground = false;
    askedQuestion = question;
    profile.Ask(question, OverItsOwnBackground);
}

// The birthday asks for the month and then for the day; "?" in either is no birthday. A new creator name is typed on the keyboard.
void EditorScreen::HandleDialogAnswers()
{
    if (!askedQuestion)
        return;

    std::optional<ProfileAnswer> answer = profile.TakeAnswer();
    if (!answer)
        return;

    ProfileQuestion question = *askedQuestion;
    askedQuestion.reset();
    if (answer->wasCancelled)
        return;

    switch (question)
    {
    case ProfileQuestion::Month:
        if (answer->choice == ProfileDialog::NoBirthdayChoice)
        {
            editor->SetBirthday(0, 0);
            break;
        }
        chosenBirthMonth = answer->choice;
        Ask(ProfileQuestion::Day);
        return;
    case ProfileQuestion::Day:
        if (answer->choice == ProfileDialog::NoBirthdayChoice)
            editor->SetBirthday(0, 0);
        else
            editor->SetBirthday(chosenBirthMonth, answer->choice);
        break;
    case ProfileQuestion::Gender:
        editor->SetGender(answer->choice == ProfileDialog::FemaleChoice ? Mii::Gender::Female : Mii::Gender::Male);
        break;
    case ProfileQuestion::FavoriteColor:
        editor->ChooseColor(ColorPart::Favorite, answer->choice);
        break;
    case ProfileQuestion::Creator:
        if (answer->choice == ProfileDialog::NewCreatorChoice)
        {
            OpenKeyboard(EnterCreatorName, editor->Record().creatorName, true);
            return;
        }
        editor->SetCreatorName(creatorChoices.at(answer->choice));
        break;
    case ProfileQuestion::Mingle:
        editor->SetMingles(answer->choice == ProfileDialog::MingleChoice);
        break;
    case ProfileQuestion::HowToMake:
        break;
    }
    RefreshProfile();
}

void EditorScreen::HandleKeyboardAnswer()
{
    std::optional<SystemMenu::KeyboardAnswer> answer = keyboard.TakeAnswer();
    if (!answer || !answer->wasConfirmed)
        return;

    if (isTypingCreator)
        editor->SetCreatorName(answer->text);
    else
        editor->SetNickname(answer->text);
    RefreshProfile();
}

void EditorScreen::HandleMessageAnswer()
{
    if (pending == Pending::Nothing)
        return;

    std::optional<MessageAnswer> answer = message.TakeAnswer();
    if (!answer)
        return;

    Pending answered = pending;
    pending = Pending::Nothing;

    if (answered == Pending::QuitChoice)
    {
        if (*answer == MessageAnswer::FirstChoice)
        {
            Save();
        }
        else if (*answer == MessageAnswer::SecondChoice)
        {
            pending = Pending::QuitWithoutSaving;
            message.AskQuestion(MiiMessage::QuitWithoutSaving);
        }
        return;
    }

    if (answered == Pending::QuitWithoutSaving && *answer == MessageAnswer::Yes)
    {
        services.sounds.Request(NoSaveSound);
        result = EditorResult{false, editedSlot};
        editor.reset();
        return;
    }

    if (answered == Pending::CompleteProfile)
        ShowPage(EditorPage::Profile);
}

void EditorScreen::AskQuit()
{
    services.sounds.Request(QuitSound);
    pending = Pending::QuitChoice;
    message.OfferThreeChoices(MiiMessage::SaveAndQuit, MiiMessage::QuitWithoutSavingButton, MiiMessage::Cancel);
}

// A Mii needs a nickname before it can be saved (the face library refuses a record without one). A new Mii goes into the first free slot.
void EditorScreen::Save()
{
    Mii::MiiRecord record = editor->Record();
    if (record.name.empty())
    {
        pending = Pending::CompleteProfile;
        message.ShowNotice(MiiMessage::CompleteTheProfile);
        return;
    }

    if (!editedSlot)
    {
        if (data.IsFull())
        {
            pending = Pending::PlazaIsFull;
            message.ShowNotice(MiiMessage::PlazaIsFullCannotCreate);
            return;
        }

        editedSlot = data.AddMii(record);
    }
    else
    {
        data.ReplaceMii(*editedSlot, record);
    }

    services.sounds.Request(SaveSound);
    result = EditorResult{true, editedSlot};
    editor.reset();
}

bool EditorScreen::IsPressed(std::string_view buttonPane) const
{
    PlayerOneInput input = ReadPlayerOne(services);
    if (!input.pointer || !input.WasPressed(Input::RemoteButtons::A))
        return false;

    return layout->IsPointInPane(buttonPane, *input.pointer);
}

// The sparkles of a changed part (Nc_change_parts01) at a height above the head of the Mii.
void EditorScreen::SparkleAtThePart(float heightAboveTheHead)
{
    Formats::Vector3 head = figures.StandingHeadPosition(editor->Record());
    head.y += heightAboveTheHead;
    effects.Start(MiiEffects::ChangedPartSparkles, Graphics::MakeTranslation(head));
}

std::optional<ColorPart> EditorScreen::ColorPartOf(EditorPart part) const
{
    switch (part)
    {
    case EditorPart::FaceShape:
    case EditorPart::MakeUp:
        return ColorPart::Skin;
    case EditorPart::Hair:
        return ColorPart::Hair;
    case EditorPart::Eyebrows:
        return ColorPart::Eyebrows;
    case EditorPart::Eyes:
        return ColorPart::Eyes;
    case EditorPart::Mouth:
        return ColorPart::Mouth;
    case EditorPart::Glasses:
        return ColorPart::Glasses;
    case EditorPart::Mustache:
    case EditorPart::Beard:
        return ColorPart::FacialHair;
    default:
        return std::nullopt;
    }
}

}
