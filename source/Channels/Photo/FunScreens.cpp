#include "Channels/Photo/MoodFilters.h"
#include "Channels/Photo/PhotoPictures.h"
#include "Channels/Photo/PhotoSession.h"
#include "Channels/Photo/ShownPicture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

// Fun! (layout "play") and two of its games: Mood (layout "filter") and Doodle (layouts "paint" and "paint_erase").
// A changed photo stays changed while the channel shows the card; the view, the slide show and Post use it.

namespace Channels
{

namespace
{

// The sounds of the moods, as rev_photo.brsar names them: BIHAKU is Brighten, GAAN is Zap!, BIGURO Black and White.
constexpr std::string_view BrightenSoundStart = "SE_FUNIKI_BIHAKU";
constexpr std::string_view HardBoiledSoundStart = "SE_FUNIKI_HARDBOILED";
constexpr std::string_view BlackAndWhiteSound = "SE_FUNIKI_BIGURO";
constexpr std::string_view ZapSound = "SE_FUNIKI_GAAN";
constexpr std::string_view PenSound = "SE_RAKU_PEN_START";
constexpr std::string_view ColorSound = "SE_RAKU_COLOR_SEL";
constexpr std::string_view StampSound = "SE_RAKU_STAMP_SEL";
constexpr std::string_view EraserSound = "SE_RAKU_ERASER_SEL";
constexpr std::string_view PickerSound = "SE_RAKU_SPUIT_SEL";
constexpr std::string_view ScissorsSound = "SE_RAKU_HASAMI_SELECT";
constexpr std::string_view UndoAllSound = "SE_RAKU_ALL_CLEAR";
constexpr std::string_view PickedColorSound = "SE_RAKU_SPUIT_ON";
constexpr std::string_view StampPutSound = "SE_RAKU_STAMP_KISS";
constexpr std::string_view CutSound = "SE_RAKU_HASAMI_START";
constexpr std::string_view PasteSound = "SE_RAKU_HASAMI_END";

// The texts of the steps of Brighten and Hard-boiled: filter_aTUS0 is the name alone, filter_aTUS1 to 8 the name with the step.
constexpr std::string_view BrightenStepText = "filter_aTUS";
constexpr std::string_view HardBoiledStepText = "filter_dTUS";

class FunScreen : public PhotoScreen
{
public:
    FunScreen(PhotoSession& photoSession, int index)
        : session(photoSession)
        , fileIndex(index)
        , buttons(session.MakeLayout("play"))
    {
    }

    void Update() override
    {
        buttons->Calculate();
        ChannelServices& services = session.Services();
        if (session.IsButtonPressed(*buttons, "filter"))
            Open(MakeMoodScreen(session, fileIndex));
        else if (session.IsButtonPressed(*buttons, "paint"))
            Open(MakeDoodleScreen(session, fileIndex));
        else if (session.IsButtonPressed(*buttons, "puzzle"))
            Open(MakePuzzleScreen(session, fileIndex));
        else if (session.IsButtonPressed(*buttons, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            session.GoTo(MakeViewScreen(session, fileIndex));
        }
    }

    void Draw() override
    {
        buttons->Draw();
    }

    std::string_view Name() const override
    {
        return "Fun";
    }

private:
    void Open(std::unique_ptr<PhotoScreen> screen)
    {
        session.Services().sounds.Request(PhotoSound::Decide);
        session.GoTo(std::move(screen));
    }

    PhotoSession& session;
    int fileIndex;
    std::unique_ptr<SystemMenu::MenuLayout> buttons;
};

class MoodScreen : public PhotoScreen
{
public:
    MoodScreen(PhotoSession& photoSession, int index)
        : session(photoSession)
        , fileIndex(index)
        , buttons(session.MakeLayout("filter"))
    {
        original = session.OriginalPhotoOfFile(fileIndex).value_or(MissingPhotoPicture());
        current = session.PhotoOfFile(fileIndex).value_or(original);
        ShowPhoto();
        ShowSteps();
    }

    void Update() override
    {
        buttons->Calculate();
        if (session.IsButtonPressed(*buttons, "filter_a"))
        {
            brightenStep = std::min(brightenStep + 1, StrongestMoodStep);
            ApplyMood(Brighten(original, brightenStep), std::string(BrightenSoundStart) + std::to_string(brightenStep));
        }
        else if (session.IsButtonPressed(*buttons, "filter_b"))
        {
            ApplyMood(BlackAndWhite(original), BlackAndWhiteSound);
        }
        else if (session.IsButtonPressed(*buttons, "filter_c"))
        {
            ApplyMood(Zap(original), ZapSound);
        }
        else if (session.IsButtonPressed(*buttons, "filter_d"))
        {
            hardBoiledStep = std::min(hardBoiledStep + 1, StrongestMoodStep);
            ApplyMood(HardBoiled(original, hardBoiledStep), std::string(HardBoiledSoundStart) + std::to_string(hardBoiledStep));
        }
        else if (session.IsButtonPressed(*buttons, "clear"))
        {
            session.Services().sounds.Request(PhotoSound::Cancel);
            session.ForgetChanges(fileIndex);
            current = original;
            brightenStep = 0;
            hardBoiledStep = 0;
            ShowPhoto();
            ShowSteps();
        }
        else if (session.IsButtonPressed(*buttons, "back"))
        {
            session.Services().sounds.Request(PhotoSound::Cancel);
            session.GoTo(MakeViewScreen(session, fileIndex));
        }
    }

    void Draw() override
    {
        buttons->Draw();
        shown->DrawInside(session.BoxOfPane(*buttons, "picture"));
    }

    std::string_view Name() const override
    {
        return "Mood";
    }

private:
    // Every mood starts from the photo as the card holds it; choosing Brighten or Hard-boiled again makes it one step stronger.
    void ApplyMood(const Formats::RgbaImage& changed, std::string_view sound)
    {
        session.Services().sounds.Request(sound);
        current = changed;
        session.ChangePhoto(fileIndex, current);
        ShowPhoto();
        ShowSteps();
    }

    void ShowPhoto()
    {
        shown = std::make_unique<ShownPicture>(session.Services().display.Device(), current);
    }

    void ShowSteps()
    {
        for (int step = 0; step <= StrongestMoodStep; ++step)
        {
            buttons->PaneNamed(std::string(BrightenStepText) + std::to_string(step)).isVisible = step == brightenStep;
            buttons->PaneNamed(std::string(HardBoiledStepText) + std::to_string(step)).isVisible = step == hardBoiledStep;
        }
    }

    PhotoSession& session;
    int fileIndex;
    std::unique_ptr<SystemMenu::MenuLayout> buttons;
    Formats::RgbaImage original;
    Formats::RgbaImage current;
    std::unique_ptr<ShownPicture> shown;
    int brightenStep = 0;
    int hardBoiledStep = 0;
};

// The tools of Doodle. The pens are the colors of the layout, the stamps pictures of TPLPhoto.tpl.
enum class DoodleTool
{
    Pen,
    Eraser,
    ColorPicker,
    Stamp,
    Scissors,
};

struct ColorButton
{
    std::string_view name;
    Formats::Rgba8Pixel color;
};

constexpr std::array<ColorButton, 7> PenColors = {{
    {"col_red", {230, 30, 40, 255}},
    {"col_yellow", {250, 220, 30, 255}},
    {"col_green", {40, 190, 60, 255}},
    {"col_blue", {30, 90, 230, 255}},
    {"col_pink", {250, 120, 180, 255}},
    {"col_black", {0, 0, 0, 255}},
    {"col_white", {255, 255, 255, 255}},
}};

struct StampButton
{
    std::string_view name;
    int textureIndex;
};

// The stamps and their pictures in TPLPhoto.tpl: the lips, the sunglasses, the sparkle and the heart.
constexpr std::array<StampButton, 4> Stamps = {{{"kiss", 14}, {"glass", 17}, {"kira", 0}, {"heart", 2}}};

// The size of the pen and of the piece that the scissors cut, in pixels of the screen (Assumed, CHN-33).
constexpr float PenRadiusOnScreen = 3;
constexpr float SmallestPenRadius = 1;
constexpr float CutPieceSideOnScreen = 80;
constexpr std::array<float, 2> DoodleZooms = {1, 2};

class DoodleScreen : public PhotoScreen
{
public:
    DoodleScreen(PhotoSession& photoSession, int index)
        : session(photoSession)
        , fileIndex(index)
        , buttons(session.MakeLayout("paint"))
        , undoQuestion(session.MakeLayout("paint_erase"))
    {
        wasChangedAtStart = session.IsPhotoChanged(fileIndex);
        atStart = session.PhotoOfFile(fileIndex).value_or(MissingPhotoPicture());
        current = atStart;
        ShowPhoto();
    }

    void Update() override
    {
        buttons->Calculate();
        undoQuestion->Calculate();
        if (isAskingToUndo)
        {
            UpdateUndoQuestion();
            return;
        }

        if (ChooseTool())
            return;

        if (session.IsButtonPressed(*buttons, "back"))
        {
            session.Services().sounds.Request(PhotoSound::Cancel);
            session.GoTo(MakeViewScreen(session, fileIndex));
            return;
        }

        UseTool();
    }

    void Draw() override
    {
        shown->DrawInside(PhotoBox(), 1);
        buttons->Draw();
        if (isAskingToUndo)
            undoQuestion->Draw();
    }

    std::string_view Name() const override
    {
        return isAskingToUndo ? "Undo all changes?" : "Doodle";
    }

private:
    PictureBox PhotoBox()
    {
        Graphics::Canvas canvas = session.Services().display.GetCanvas();
        PictureBox box = {0, 0, canvas.width, canvas.height};
        float zoom = DoodleZooms[zoomIndex];
        float scale = std::min(box.width / current.width, box.height / current.height) * zoom;
        return {0, 0, current.width * scale, current.height * scale};
    }

    bool ChooseTool()
    {
        ChannelServices& services = session.Services();
        for (const ColorButton& button : PenColors)
        {
            if (session.IsButtonPressed(*buttons, button.name))
            {
                services.sounds.Request(ColorSound);
                tool = DoodleTool::Pen;
                penColor = button.color;
                return true;
            }
        }

        for (const StampButton& button : Stamps)
        {
            if (session.IsButtonPressed(*buttons, button.name))
            {
                services.sounds.Request(StampSound);
                tool = DoodleTool::Stamp;
                stampTexture = button.textureIndex;
                return true;
            }
        }

        if (session.IsButtonPressed(*buttons, "erase"))
            return SelectTool(DoodleTool::Eraser, EraserSound);
        if (session.IsButtonPressed(*buttons, "syringe"))
            return SelectTool(DoodleTool::ColorPicker, PickerSound);
        if (session.IsButtonPressed(*buttons, "cut"))
            return SelectTool(DoodleTool::Scissors, ScissorsSound);

        if (session.IsButtonPressed(*buttons, "erase_all"))
        {
            services.sounds.Request(PhotoSound::Decide);
            isAskingToUndo = true;
            return true;
        }

        if (session.IsButtonPressed(*buttons, "zoom_in") || session.IsButtonPressed(*buttons, "zoom_out"))
        {
            zoomIndex = 1 - zoomIndex;
            services.sounds.Request(zoomIndex == 1 ? PhotoSound::ZoomIn : PhotoSound::ZoomOut);
            return true;
        }
        return false;
    }

    bool SelectTool(DoodleTool newTool, std::string_view sound)
    {
        session.Services().sounds.Request(sound);
        tool = newTool;
        cutPiece = std::nullopt;
        return true;
    }

    // Where the pointer of a player who holds A is on the photo, in its pixels; nothing outside the photo.
    std::optional<Formats::Vector2> PhotoPixelUnderHeldA()
    {
        ChannelServices& services = session.Services();
        for (int playerIndex = 0; playerIndex < SystemMenu::PlayerCount; ++playerIndex)
        {
            const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(playerIndex);
            if (!player || !player->state.pointerIsValid)
                continue;

            bool isHoldingA = (player->state.buttonsHeld & Input::RemoteButtons::A) != 0;
            if (!isHoldingA)
                continue;

            Formats::Vector2 place = services.display.PointerToCanvas(player->state.pointer);
            PictureBox box = PhotoBox();
            float boxLeft = box.centerX - box.width / 2;
            float boxTop = box.centerY + box.height / 2;
            float x = (place.x - boxLeft) / box.width * current.width;
            float y = (boxTop - place.y) / box.height * current.height;
            if (!IsInsidePhoto(x, y))
                return std::nullopt;

            return Formats::Vector2{x, y};
        }
        return std::nullopt;
    }

    bool IsInsidePhoto(float x, float y) const
    {
        bool isInsideAcross = x >= 0 && x < current.width;
        bool isInsideDownward = y >= 0 && y < current.height;
        return isInsideAcross && isInsideDownward;
    }

    std::size_t PixelIndex(int x, int y) const
    {
        return static_cast<std::size_t>(y) * current.width + static_cast<std::size_t>(x);
    }

    float PixelsPerScreenUnit()
    {
        return current.width / PhotoBox().width;
    }

    void UseTool()
    {
        std::optional<Formats::Vector2> pixel = PhotoPixelUnderHeldA();
        if (!pixel)
        {
            lastPenPixel = std::nullopt;
            return;
        }

        bool isNewPress = !lastPenPixel.has_value();
        if (tool == DoodleTool::Pen || tool == DoodleTool::Eraser)
            DrawStroke(*pixel);
        else if (isNewPress && tool == DoodleTool::ColorPicker)
            PickColor(*pixel);
        else if (isNewPress && tool == DoodleTool::Stamp)
            PutStamp(*pixel);
        else if (isNewPress && tool == DoodleTool::Scissors)
            UseScissors(*pixel);

        lastPenPixel = pixel;
    }

    void DrawStroke(Formats::Vector2 pixel)
    {
        if (!lastPenPixel)
            session.Services().sounds.Request(PenSound);

        Formats::Vector2 from = lastPenPixel.value_or(pixel);
        float radius = std::max(PenRadiusOnScreen * PixelsPerScreenUnit(), SmallestPenRadius);
        float length = std::hypot(pixel.x - from.x, pixel.y - from.y);
        float dotSpacing = std::max(radius / 2, 1.0f);
        int dots = std::max(1, static_cast<int>(length / dotSpacing));
        for (int dot = 0; dot <= dots; ++dot)
        {
            float share = static_cast<float>(dot) / dots;
            Formats::Vector2 center = {from.x + (pixel.x - from.x) * share, from.y + (pixel.y - from.y) * share};
            PaintDot(center, radius);
        }
        Changed();
    }

    void PaintDot(Formats::Vector2 center, float radius)
    {
        int left = std::max(0, static_cast<int>(center.x - radius));
        int right = std::min(static_cast<int>(current.width) - 1, static_cast<int>(center.x + radius));
        int top = std::max(0, static_cast<int>(center.y - radius));
        int bottom = std::min(static_cast<int>(current.height) - 1, static_cast<int>(center.y + radius));
        for (int y = top; y <= bottom; ++y)
        {
            for (int x = left; x <= right; ++x)
            {
                if (std::hypot(x - center.x, y - center.y) > radius)
                    continue;

                current.pixels[PixelIndex(x, y)] = tool == DoodleTool::Eraser ? OriginalPixel(x, y) : penColor;
            }
        }
    }

    // The eraser brings back the photo as the card holds it (Mood included, which is not a doodle).
    Formats::Rgba8Pixel OriginalPixel(int x, int y) const
    {
        bool hasSameSize = atStart.width == current.width && atStart.height == current.height;
        if (!hasSameSize)
            return current.pixels[PixelIndex(x, y)];

        return atStart.pixels[PixelIndex(x, y)];
    }

    void PickColor(Formats::Vector2 pixel)
    {
        session.Services().sounds.Request(PickedColorSound);
        penColor = current.pixels[PixelIndex(static_cast<int>(pixel.x), static_cast<int>(pixel.y))];
        tool = DoodleTool::Pen;
    }

    void PutStamp(Formats::Vector2 pixel)
    {
        session.Services().sounds.Request(StampPutSound);
        const Formats::RgbaImage& stamp = session.ChannelTexture(stampTexture);
        float scale = PixelsPerScreenUnit();
        PastePicture(stamp, pixel, scale);
        Changed();
    }

    // The first press cuts a square piece around the pointer, the next one puts it down there, as often as wanted.
    void UseScissors(Formats::Vector2 pixel)
    {
        if (!cutPiece)
        {
            session.Services().sounds.Request(CutSound);
            cutPiece = CutSquare(pixel, CutPieceSideOnScreen * PixelsPerScreenUnit());
            return;
        }

        session.Services().sounds.Request(PasteSound);
        PastePicture(*cutPiece, pixel, 1);
        Changed();
    }

    Formats::RgbaImage CutSquare(Formats::Vector2 center, float side)
    {
        int half = static_cast<int>(side / 2);
        Formats::RgbaImage piece{static_cast<std::uint32_t>(2 * half), static_cast<std::uint32_t>(2 * half), {}};
        for (int y = -half; y < half; ++y)
        {
            for (int x = -half; x < half; ++x)
            {
                int sourceX = std::clamp(static_cast<int>(center.x) + x, 0, static_cast<int>(current.width) - 1);
                int sourceY = std::clamp(static_cast<int>(center.y) + y, 0, static_cast<int>(current.height) - 1);
                piece.pixels.push_back(current.pixels[PixelIndex(sourceX, sourceY)]);
            }
        }
        return piece;
    }

    // Puts a picture onto the photo, centered on a pixel and magnified by scale, mixed by its alpha.
    void PastePicture(const Formats::RgbaImage& picture, Formats::Vector2 center, float scale)
    {
        int width = std::max(1, static_cast<int>(picture.width * scale));
        int height = std::max(1, static_cast<int>(picture.height * scale));
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int targetX = static_cast<int>(center.x) - width / 2 + x;
                int targetY = static_cast<int>(center.y) - height / 2 + y;
                if (!IsInsidePhoto(static_cast<float>(targetX), static_cast<float>(targetY)))
                    continue;

                std::size_t sourceX = static_cast<std::size_t>(x * picture.width / width);
                std::size_t sourceY = static_cast<std::size_t>(y * picture.height / height);
                const Formats::Rgba8Pixel& source = picture.pixels[sourceY * picture.width + sourceX];
                Formats::Rgba8Pixel& target = current.pixels[PixelIndex(targetX, targetY)];
                target = Mix(target, source);
            }
        }
    }

    static Formats::Rgba8Pixel Mix(const Formats::Rgba8Pixel& below, const Formats::Rgba8Pixel& above)
    {
        constexpr int Opaque = 255;
        auto mixed = [&](Formats::Byte under, Formats::Byte over) {
            return static_cast<Formats::Byte>((over * above.alpha + under * (Opaque - above.alpha)) / Opaque);
        };
        return {mixed(below.red, above.red), mixed(below.green, above.green), mixed(below.blue, above.blue), below.alpha};
    }

    void Changed()
    {
        session.ChangePhoto(fileIndex, current);
        ShowPhoto();
    }

    void ShowPhoto()
    {
        shown = std::make_unique<ShownPicture>(session.Services().display.Device(), current);
    }

    // "Undo all changes?" brings back the photo as it was when Doodle opened.
    void UpdateUndoQuestion()
    {
        if (session.IsButtonPressed(*undoQuestion, "yes"))
        {
            session.Services().sounds.Request(UndoAllSound);
            current = atStart;
            if (wasChangedAtStart)
                session.ChangePhoto(fileIndex, current);
            else
                session.ForgetChanges(fileIndex);
            ShowPhoto();
            isAskingToUndo = false;
        }
        else if (session.IsButtonPressed(*undoQuestion, "no"))
        {
            session.Services().sounds.Request(PhotoSound::Cancel);
            isAskingToUndo = false;
        }
    }

    PhotoSession& session;
    int fileIndex;
    std::unique_ptr<SystemMenu::MenuLayout> buttons;
    std::unique_ptr<SystemMenu::MenuLayout> undoQuestion;
    bool wasChangedAtStart = false;
    Formats::RgbaImage atStart;
    Formats::RgbaImage current;
    std::unique_ptr<ShownPicture> shown;
    DoodleTool tool = DoodleTool::Pen;
    Formats::Rgba8Pixel penColor = PenColors[0].color;
    int stampTexture = Stamps[0].textureIndex;
    std::optional<Formats::Vector2> lastPenPixel;
    std::optional<Formats::RgbaImage> cutPiece;
    int zoomIndex = 0;
    bool isAskingToUndo = false;
};

}

std::unique_ptr<PhotoScreen> MakeFunScreen(PhotoSession& session, int fileIndex)
{
    return std::make_unique<FunScreen>(session, fileIndex);
}

std::unique_ptr<PhotoScreen> MakeMoodScreen(PhotoSession& session, int fileIndex)
{
    return std::make_unique<MoodScreen>(session, fileIndex);
}

std::unique_ptr<PhotoScreen> MakeDoodleScreen(PhotoSession& session, int fileIndex)
{
    return std::make_unique<DoodleScreen>(session, fileIndex);
}

}
