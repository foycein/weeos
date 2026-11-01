#pragma once

#include "Channels/ChannelServices.h"

#include "SystemMenu/MenuLayout.h"

#include "Formats/NumberedMessageFile.h"
#include "Layout/LayoutArchive.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Channels
{

// The questions of the layout profileNigaoe: the first two when a new Mii is made, the others from the profile page of the editor.
enum class ProfileQuestion
{
    // "Select a gender." Male (0) or female (1).
    Gender,

    // "How do you want to make a Mii?" Start from scratch (0) or choose a look-alike (1).
    HowToMake,

    // "Enter a birth date." Month 1 to 12, or 0 for "?"; then the day 1 to 31, or 0.
    Month,
    Day,

    // "Choose a favorite color." 0 to 11.
    FavoriteColor,

    // "Choose a creator." One of the names shown (0 to 2), or 3 for a new name.
    Creator,

    // "Do you want to let this Mii mingle?" Mingle (1) or don't mingle (0).
    Mingle,
};

struct ProfileAnswer
{
    // Cancel, or Back on the question how to make a Mii.
    bool wasCancelled = false;
    int choice = 0;
};

// The dialogs of profileNigaoe: every question is a group of panes of the layout, shown under the words of the question at the top.
class ProfileDialog
{
public:
    static constexpr int CreatorChoiceCount = 3;
    static constexpr int NewCreatorChoice = 3;

    // The services, the archive and the messages must outlive the dialog.
    ProfileDialog(ChannelServices& channelServices, const Layout::LayoutArchive& layouts, const Formats::NumberedMessageFile& messages);

    // The question opens over a background of its own (the first questions of a new Mii) or over the editor.
    void Ask(ProfileQuestion question, bool hasBackground);

    // The names that "Choose a creator" offers: the creators of the Miis of the plaza, at most three.
    void SetCreatorNames(const std::vector<std::u16string>& names);

    bool IsOpen() const;
    std::optional<ProfileQuestion> OpenQuestion() const;
    std::optional<ProfileAnswer> TakeAnswer();

    // Where the middle of a pane is in the picture, in layout units, for the tests that point at it. (0, 0) for a pane that is hidden.
    Formats::Vector2 PlaceOf(std::string_view paneName) const;

    void Update();
    void Draw();

    // The pane of a choice, for the tests: the button that answers the question with this choice.
    std::string ButtonOf(ProfileQuestion question, int choice) const;
    std::string CancelButtonOf(ProfileQuestion question) const;

private:
    struct Choice
    {
        std::string buttonPane;
        int choice;
    };

    std::vector<Choice> ChoicesOf(ProfileQuestion question) const;
    void ShowGroupOf(ProfileQuestion question);
    void Answer(ProfileAnswer answer);
    bool IsPressed(const std::string& buttonPane) const;

    ChannelServices& services;
    const Formats::NumberedMessageFile& texts;
    std::unique_ptr<SystemMenu::MenuLayout> layout;
    std::optional<ProfileQuestion> openQuestion;
    std::optional<ProfileAnswer> answer;
    std::size_t creatorNameCount = 0;
};

}
