#pragma once

#include "SystemMenu/MenuServices.h"
#include "SystemMenu/MessageBoard/BoardPosting.h"
#include "SystemMenu/MessageBoard/LetterPaper.h"
#include "SystemMenu/Screen.h"

#include "Layout/LayoutArchive.h"

#include <memory>
#include <optional>
#include <string>

namespace SystemMenu
{

class BottomBarScreen;
struct MessageBoardData;

// The longest memo: the writer copies at most 0x400 characters of the keyboard into the message (0x8140C69C).
constexpr std::size_t LongestMemo = 0x400;

// "Write a memo" (screen 0x0A, the class 0x8140B41C to 0x8140CD44). The paper of a memo with "Memo" at the top, "Write a memo" where the
// text goes and "<-Add a Mii" beside the place of the face. Pressing the paper opens the soft keyboard; pressing the face opens the
// Mii chooser (0x1C). Post on the bar puts the memo on the day that the board shows; Back throws it away.
// The original sends the memo to its own console through WiiConnect24 and the mail service puts it on the board (0x8140C92C, 0x81342670);
// WeeOS has no WiiConnect24 and puts the same record into the database at once.
class MemoWriterScreen : public Screen
{
public:
    explicit MemoWriterScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    // The paper stays in view under the Mii chooser.
    bool DrawsWithChildren() const override;

    MenuLayout& PaperLayout();
    const std::u16string& Text() const;
    const std::optional<MiiData>& Mii() const;

    // The memo that was posted, once it is in the database.
    const std::optional<StoredMessage>& PostedMemo() const;

protected:
    bool RunMain() override;

private:
    enum class WriterState
    {
        Opening,
        Writing,
        Typing,
        ChoosingMii,
        TellingThereAreNoMiis,
        Leaving,
    };

    void ShowTheText();
    void ShowTheMii();
    void RunWriting();
    bool HandleBarPress(Formats::Vector2 place);
    void OpenTheKeyboard(int playerIndex);
    void OpenTheMiiChooser();
    void Post();
    void Cancel();
    BottomBarScreen* Bar();

    MenuServices& services;
    MessageBoardData& data;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<LetterPaper> paper;
    WriterState state = WriterState::Opening;
    std::size_t openAnimation = 0;
    std::size_t cancelAnimation = 0;
    std::size_t postAnimation = 0;
    std::size_t leaveAnimation = 0;
    std::u16string text;
    std::optional<MiiData> mii;
    std::optional<StoredMessage> postedMemo;
};

}
