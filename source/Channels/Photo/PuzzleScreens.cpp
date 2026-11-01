#include "Channels/Photo/PhotoSession.h"
#include "Channels/Photo/PuzzleBoard.h"
#include "Channels/Photo/ShownPicture.h"

#include "Channels/ChannelText.h"

#include <array>
#include <format>
#include <random>

// Puzzle of Fun!: the photo cut into pieces that are mixed; A picks a piece up and puts it where another one lies, which takes its
// place. Layouts: puzzle_set (the number of pieces), puzzle (the board), puzzle_hint ("Cheat": the whole photo), puzzle_end
// (Congratulations!), puzzle_piece (the number of pieces may be chosen from now on), puzzle_record (the best times).

namespace Channels
{

namespace
{

struct PuzzleSize
{
    std::string_view button;
    std::string_view recordTime;
    int columns;
    int rows;
};

// The five sizes of puzzle_set: 6, 12, 24, 48 and 192 pieces, the board in the shape of a 4:3 photo.
constexpr std::array<PuzzleSize, PuzzleSizeCount> PuzzleSizes = {{
    {"level_a", "time_a1", 3, 2},
    {"level_c", "time_c1", 4, 3},
    {"level_e", "time_e1", 6, 4},
    {"level_f", "time_f1", 8, 6},
    {"level_g", "", 16, 12},
}};

constexpr float BoardTop = 150;
constexpr float BoardBottom = -150;
constexpr std::string_view PickSound = "SE_PUZZULE_GET";
constexpr std::string_view PutSound = "SE_PUZZULE_PUT";
constexpr std::string_view SolvedMusic = "BGM_PUZZULE_FANFARE";
constexpr std::string_view PuzzleMusic = "BGM_PUZZULE";
constexpr std::string_view NewRecordMessage = "textTUS1A";
constexpr std::string_view SolvedMessage = "textTUS0A";
constexpr int StepsPerSecond = 60;
constexpr int SecondsPerMinute = 60;

std::u16string TimeText(int steps)
{
    int seconds = steps / StepsPerSecond;
    return AsciiToText(std::format("{}:{:02}", seconds / SecondsPerMinute, seconds % SecondsPerMinute));
}

class PuzzleScreen : public PhotoScreen
{
public:
    enum class Stage
    {
        ChoosingSize,
        Playing,
        Cheating,
        Solved,
        PieceCountUnlocked,
        Records,
    };

    PuzzleScreen(PhotoSession& photoSession, int index)
        : session(photoSession)
        , fileIndex(index)
        , sizeChoice(session.MakeLayout("puzzle_set"))
        , board(session.MakeLayout("puzzle"))
        , cheat(session.MakeLayout("puzzle_hint"))
        , solved(session.MakeLayout("puzzle_end"))
        , unlocked(session.MakeLayout("puzzle_piece"))
        , records(session.MakeLayout("puzzle_record"))
        , random(std::random_device()())
    {
        Formats::RgbaImage photo = session.PhotoOfFile(fileIndex).value_or(Formats::RgbaImage{1, 1, {{0, 0, 0, 255}}});
        picture = std::make_unique<ShownPicture>(session.Services().display.Device(), photo);
        photoAspect = static_cast<float>(photo.width) / static_cast<float>(photo.height);
        session.Services().sounds.Request(PuzzleMusic);

        // Until the first puzzle is solved there is only the puzzle of 6 pieces.
        if (session.storage.Settings().mayChoosePieceCount)
            stage = Stage::ChoosingSize;
        else
            StartPuzzle(0);
    }

    void Update() override
    {
        for (SystemMenu::MenuLayout* layout : {sizeChoice.get(), board.get(), cheat.get(), solved.get(), unlocked.get(), records.get()})
            layout->Calculate();

        if (stage == Stage::ChoosingSize)
            UpdateSizeChoice();
        else if (stage == Stage::Playing)
            UpdatePlaying();
        else if (stage == Stage::Cheating)
            UpdateCheat();
        else if (stage == Stage::Solved)
            UpdateSolved();
        else if (stage == Stage::PieceCountUnlocked)
            UpdateUnlocked();
        else
            UpdateRecords();
    }

    void Draw() override
    {
        if (stage == Stage::ChoosingSize)
        {
            sizeChoice->Draw();
            picture->DrawInside(session.BoxOfPane(*sizeChoice, "picture"));
            return;
        }

        if (stage == Stage::Records)
        {
            records->Draw();
            return;
        }

        DrawBoard();
        board->Draw();
        if (stage == Stage::Cheating)
        {
            picture->DrawInside(BoardBox());
            cheat->Draw();
        }
        else if (stage == Stage::Solved)
        {
            solved->Draw();
        }
        else if (stage == Stage::PieceCountUnlocked)
        {
            unlocked->Draw();
        }
    }

    std::string_view Name() const override
    {
        switch (stage)
        {
        case Stage::ChoosingSize:
            return "Puzzle size";
        case Stage::Playing:
            return "Puzzle";
        case Stage::Cheating:
            return "Puzzle cheat";
        case Stage::Solved:
            return "Puzzle solved";
        case Stage::PieceCountUnlocked:
            return "Puzzle sizes unlocked";
        case Stage::Records:
            return "Puzzle records";
        }
        return "Puzzle";
    }

private:
    PictureBox BoardBox() const
    {
        float height = BoardTop - BoardBottom;
        float width = height * photoAspect;
        return {0, (BoardTop + BoardBottom) / 2, width, height};
    }

    PictureBox BoxOfSlot(int slot) const
    {
        PictureBox whole = BoardBox();
        const PuzzleSize& size = PuzzleSizes[sizeIndex];
        float pieceWidth = whole.width / size.columns;
        float pieceHeight = whole.height / size.rows;
        int column = slot % size.columns;
        int row = slot / size.columns;
        float left = whole.centerX - whole.width / 2 + column * pieceWidth;
        float top = whole.centerY + whole.height / 2 - row * pieceHeight;
        return {left + pieceWidth / 2, top - pieceHeight / 2, pieceWidth, pieceHeight};
    }

    void DrawBoard()
    {
        const PuzzleSize& size = PuzzleSizes[sizeIndex];
        for (int slot = 0; slot < puzzle->PieceCount(); ++slot)
        {
            if (heldSlot == slot)
                continue;

            int piece = puzzle->PieceAt(slot);
            Formats::Vector2 topLeft = {static_cast<float>(piece % size.columns) / size.columns, static_cast<float>(piece / size.columns) / size.rows};
            Formats::Vector2 bottomRight = {topLeft.x + 1.0f / size.columns, topLeft.y + 1.0f / size.rows};
            picture->DrawPart(BoxOfSlot(slot), topLeft, bottomRight);
        }
    }

    void StartPuzzle(int newSizeIndex)
    {
        sizeIndex = newSizeIndex;
        const PuzzleSize& size = PuzzleSizes[sizeIndex];
        puzzle = std::make_unique<PuzzleBoard>(size.columns * size.rows);
        puzzle->Mix(random);
        heldSlot = std::nullopt;
        steps = 0;
        cheats = 0;
        stage = Stage::Playing;
    }

    void UpdateSizeChoice()
    {
        ChannelServices& services = session.Services();
        for (int index = 0; index < PuzzleSizeCount; ++index)
        {
            if (session.IsButtonPressed(*sizeChoice, PuzzleSizes[index].button))
            {
                services.sounds.Request("SE_PUZZ_SHUFFLE_PIECE6");
                StartPuzzle(index);
                return;
            }
        }

        if (session.IsButtonPressed(*sizeChoice, "record"))
        {
            services.sounds.Request(PhotoSound::Decide);
            ShowRecords();
        }
        else if (session.IsButtonPressed(*sizeChoice, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            services.sounds.Request(PhotoSound::MainMusic);
            session.GoTo(MakeFunScreen(session, fileIndex));
        }
    }

    void UpdatePlaying()
    {
        ++steps;
        ChannelServices& services = session.Services();
        if (session.IsButtonPressed(*board, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            LeavePuzzle();
            return;
        }

        if (session.IsButtonPressed(*board, "hint"))
        {
            services.sounds.Request("SE_KIMAGURE_HINT");
            ++cheats;
            stage = Stage::Cheating;
            return;
        }

        std::optional<Formats::Vector2> place = session.PlaceOfPress();
        if (!place)
            return;

        std::optional<int> slot = SlotAt(*place);
        if (!slot)
            return;

        if (!heldSlot)
        {
            services.sounds.Request(PickSound);
            heldSlot = slot;
            return;
        }

        services.sounds.Request(PutSound);
        puzzle->Swap(*heldSlot, *slot);
        heldSlot = std::nullopt;
        if (puzzle->IsSolved())
            Solve();
    }

    std::optional<int> SlotAt(Formats::Vector2 place) const
    {
        for (int slot = 0; slot < puzzle->PieceCount(); ++slot)
        {
            PictureBox box = BoxOfSlot(slot);
            if (std::abs(place.x - box.centerX) <= box.width / 2 && std::abs(place.y - box.centerY) <= box.height / 2)
                return slot;
        }
        return std::nullopt;
    }

    void Solve()
    {
        PhotoSettings& settings = session.storage.Settings();
        int& best = settings.puzzleBestSteps[sizeIndex];
        bool isRecord = best == 0 || steps < best;
        if (isRecord)
            best = steps;

        solved->PaneNamed(NewRecordMessage).isVisible = isRecord;
        solved->PaneNamed(SolvedMessage).isVisible = !isRecord;
        solved->SetText("message0", TimeText(steps));
        session.Services().sounds.Request(SolvedMusic);

        isFirstSolve = !settings.mayChoosePieceCount;
        settings.mayChoosePieceCount = true;
        session.storage.SaveSettings();
        stage = Stage::Solved;
    }

    void UpdateSolved()
    {
        if (!session.IsButtonPressed(*solved, "next"))
            return;

        session.Services().sounds.Request(PhotoSound::Decide);
        stage = isFirstSolve ? Stage::PieceCountUnlocked : Stage::ChoosingSize;
    }

    void UpdateUnlocked()
    {
        if (!session.IsButtonPressed(*unlocked, "next"))
            return;

        session.Services().sounds.Request(PhotoSound::Decide);
        stage = Stage::ChoosingSize;
    }

    void UpdateCheat()
    {
        if (!session.IsButtonPressed(*cheat, "back"))
            return;

        session.Services().sounds.Request(PhotoSound::Cancel);
        stage = Stage::Playing;
    }

    void ShowRecords()
    {
        const PhotoSettings& settings = session.storage.Settings();
        for (int index = 0; index < PuzzleSizeCount; ++index)
        {
            std::string_view timePane = PuzzleSizes[index].recordTime;
            if (timePane.empty() || records->FindPane(timePane) == nullptr)
                continue;

            int best = settings.puzzleBestSteps[index];
            records->SetText(timePane, best == 0 ? u"-:--" : TimeText(best));
        }
        stage = Stage::Records;
    }

    void UpdateRecords()
    {
        if (!session.IsButtonPressed(*records, "back"))
            return;

        session.Services().sounds.Request(PhotoSound::Cancel);
        stage = Stage::ChoosingSize;
    }

    void LeavePuzzle()
    {
        if (session.storage.Settings().mayChoosePieceCount)
        {
            stage = Stage::ChoosingSize;
            return;
        }

        session.Services().sounds.Request(PhotoSound::MainMusic);
        session.GoTo(MakeFunScreen(session, fileIndex));
    }

    PhotoSession& session;
    int fileIndex;
    std::unique_ptr<SystemMenu::MenuLayout> sizeChoice;
    std::unique_ptr<SystemMenu::MenuLayout> board;
    std::unique_ptr<SystemMenu::MenuLayout> cheat;
    std::unique_ptr<SystemMenu::MenuLayout> solved;
    std::unique_ptr<SystemMenu::MenuLayout> unlocked;
    std::unique_ptr<SystemMenu::MenuLayout> records;
    std::unique_ptr<ShownPicture> picture;
    std::unique_ptr<PuzzleBoard> puzzle;
    std::mt19937 random;
    float photoAspect = 1;
    Stage stage = Stage::ChoosingSize;
    int sizeIndex = 0;
    std::optional<int> heldSlot;
    int steps = 0;
    int cheats = 0;
    bool isFirstSolve = false;
};

}

std::unique_ptr<PhotoScreen> MakePuzzleScreen(PhotoSession& session, int fileIndex)
{
    return std::make_unique<PuzzleScreen>(session, fileIndex);
}

}
