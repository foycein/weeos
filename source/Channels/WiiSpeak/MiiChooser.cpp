#include "Channels/WiiSpeak/MiiChooser.h"

#include "Channels/ChannelText.h"
#include "Channels/WiiSpeak/WiiSpeakMessageIds.h"

#include <algorithm>
#include <format>
#include <string>

namespace Channels
{

namespace
{

constexpr std::size_t MiisPerPage = 10;
constexpr std::size_t CircleCount = 6;
constexpr std::size_t MostChatRoomMiis = 6;
constexpr std::size_t DigitBase = 10;

const std::string OkButton = "dbutton_00";
const std::string HelpButton = "B_dbutton_05";
const std::string LeftButton = "B_nbutton_l";
const std::string RightButton = "B_nbutton_r";

// The sounds of the lineup, by their names in MicChatSound.brsar.
constexpr std::string_view AppendSound = "SE_SEL_CLK_MII_APPEND";
constexpr std::string_view AppendMissSound = "SE_SEL_CLK_MII_APPEND_MISS";
constexpr std::string_view ClearSound = "SE_SEL_CLK_MII_CLEAR";
constexpr std::string_view NextPageSound = "SE_SEL_CLK_NEXT_GRP";
constexpr std::string_view PreviousPageSound = "SE_SEL_CLK_PREV_GRP";
constexpr std::string_view OkSound = "SE_SEL_CLK_BTN_OK";
constexpr std::string_view HelpSound = "SE_SEL_CLK_BTN_HELP";

std::string LineupButton(std::size_t place)
{
    return std::format("B_nbutton_mii_{:02}", place);
}

std::string CircleButton(std::size_t circle)
{
    return std::format("B_nbuttonc_{:02}", circle);
}

std::string Numbered(std::string_view name, std::size_t number)
{
    return std::format("{}_{:02}", name, number);
}

}

MiiChooser::MiiChooser(WiiSpeakSession& chooserSession, std::vector<Mii::MiiRecord> chosenBefore, std::size_t most)
    : session(chooserSession)
    , layout(session.services.display, session.files.SceneLayouts("mii_select", "05000_MiiSel.arc"), "05000_MiiSel")
    , chosen(std::move(chosenBefore))
    , mostChosen(most)
{
    for (std::size_t place = 0; place < MiisPerPage; ++place)
        layout.AddButton(LineupButton(place));
    for (std::size_t circle = 0; circle < CircleCount; ++circle)
        layout.AddButton(CircleButton(circle));
    for (const std::string& button : {OkButton, HelpButton, LeftButton, RightButton})
        layout.AddButton(button);

    layout.SetText("T_mii", session.Text(WiiSpeakText::ChooseMiisPrompt));
    layout.SetText("T_shadow", session.Text(WiiSpeakText::ChooseMiisPrompt));
    layout.SetText("T_button_00", session.Text(WiiSpeakText::ChooseMiisOk));
    layout.SetText("T_button_05", session.Text(WiiSpeakText::ChooseMiisHelp));
    layout.PlayAnimation("miiSel_inOut");
    layout.PlayAnimation("lr_wait");
    ShowPage();
    ShowCircles();
}

std::optional<std::vector<Mii::MiiRecord>> MiiChooser::Update()
{
    std::optional<std::vector<Mii::MiiRecord>> result;
    std::optional<std::string> pressed = layout.TakePressedButton(session.services.input);
    for (std::size_t place = 0; place < MiisPerPage; ++place)
    {
        if (pressed == LineupButton(place))
            PressLineupMii(place);
    }
    for (std::size_t circle = 0; circle < CircleCount; ++circle)
    {
        if (pressed == CircleButton(circle))
            PressCircle(circle);
    }

    if (pressed == LeftButton)
        TurnPage(-1);
    else if (pressed == RightButton)
        TurnPage(+1);
    else if (pressed == HelpButton)
    {
        session.services.sounds.Request(HelpSound);
        session.ShowMessage(WiiSpeakText::ChooseMiisHelpText, {WiiSpeakText::Ok});
    }
    else if (pressed == OkButton)
    {
        session.services.sounds.Request(OkSound);
        result = chosen;
    }

    layout.Calculate();
    return result;
}

void MiiChooser::Draw()
{
    layout.Draw();
    session.ripples.Draw();
}

SceneLayout& MiiChooser::Layout()
{
    return layout;
}

std::size_t MiiChooser::ShownPage() const
{
    return page;
}

const std::vector<Mii::MiiRecord>& MiiChooser::ChosenMiis() const
{
    return chosen;
}

void MiiChooser::PressLineupMii(std::size_t place)
{
    std::size_t index = page * MiisPerPage + place;
    if (index >= session.consoleMiis.size())
        return;

    const Mii::MiiRecord& mii = session.consoleMiis[index];
    auto inCircle = std::find_if(chosen.begin(), chosen.end(), [&mii](const Mii::MiiRecord& other) { return IsSameMii(mii, other); });
    if (inCircle != chosen.end())
    {
        chosen.erase(inCircle);
        session.services.sounds.Request(ClearSound);
    }
    else if (chosen.size() < mostChosen)
    {
        chosen.push_back(mii);
        session.services.sounds.Request(AppendSound);
        std::optional<Formats::Vector2> circle = layout.PlaceOfPane(Numbered("N_miiRootC", chosen.size() - 1));
        if (circle)
            session.ripples.StartMiiPutIntoACircle(*circle);
    }
    else
    {
        session.services.sounds.Request(AppendMissSound);
        return;
    }

    ShowPage();
    ShowCircles();
}

void MiiChooser::PressCircle(std::size_t circle)
{
    if (circle >= chosen.size())
        return;

    chosen.erase(chosen.begin() + static_cast<std::ptrdiff_t>(circle));
    session.services.sounds.Request(ClearSound);
    ShowPage();
    ShowCircles();
}

// The pages go round: right on the last page shows the first.
void MiiChooser::TurnPage(int step)
{
    std::size_t pageCount = PageCount();
    if (step > 0)
        page = (page + 1) % pageCount;
    else
        page = (page + pageCount - 1) % pageCount;

    session.services.sounds.Request(step > 0 ? NextPageSound : PreviousPageSound);
    ShowPage();
}

void MiiChooser::ShowPage()
{
    for (std::size_t place = 0; place < MiisPerPage; ++place)
    {
        std::size_t index = page * MiisPerPage + place;
        bool hasMii = index < session.consoleMiis.size();
        layout.SetButtonEnabled(LineupButton(place), hasMii);
        layout.SetVisible(Numbered("P_miiB", place), hasMii);
        if (!hasMii)
        {
            layout.ShowAnimationFrame(Numbered("nbutton_miiX", place), 0);
            continue;
        }

        const Mii::MiiRecord& mii = session.consoleMiis[index];
        layout.ShowMadeTexture(Numbered("P_miiB", place), MiiFaces::TextureNameOf(mii), session.miiFaces.FaceOf(mii));
        layout.ShowAnimationFrame(Numbered(IsChosen(mii) ? "nbutton_miiOn" : "nbutton_miiOff", place), 0);
    }

    std::size_t shownPage = page + 1;
    std::size_t tens = shownPage / DigitBase;
    layout.SetText("T_no_00", tens > 0 ? AsciiToText(std::to_string(tens)) : u"");
    layout.SetText("T_no_01", AsciiToText(std::to_string(shownPage % DigitBase)));
    layout.SetText("T_no_03", AsciiToText(std::to_string(PageCount())));
}

void MiiChooser::ShowCircles()
{
    for (std::size_t circle = 0; circle < CircleCount; ++circle)
    {
        bool hasMii = circle < chosen.size() && circle < mostChosen;
        layout.SetVisible(Numbered("P_miiC", circle), hasMii);
        layout.SetVisible(Numbered("N_miiRootC", circle), circle < mostChosen);
        layout.SetButtonEnabled(CircleButton(circle), hasMii);
        if (hasMii)
            layout.ShowMadeTexture(Numbered("P_miiC", circle), MiiFaces::TextureNameOf(chosen[circle]), session.miiFaces.FaceOf(chosen[circle]));
    }
}

bool MiiChooser::IsChosen(const Mii::MiiRecord& mii) const
{
    return std::any_of(chosen.begin(), chosen.end(), [&mii](const Mii::MiiRecord& other) { return IsSameMii(mii, other); });
}

std::size_t MiiChooser::PageCount() const
{
    std::size_t pages = (session.consoleMiis.size() + MiisPerPage - 1) / MiisPerPage;
    return std::max<std::size_t>(pages, 1);
}

ChooseMiisScreen::ChooseMiisScreen(WiiSpeakSession& chooseSession)
    : session(chooseSession)
    , chooser(chooseSession, chooseSession.saveData.ChosenMiis(), MostChatRoomMiis)
{
}

std::optional<WiiSpeakScreenName> ChooseMiisScreen::Update()
{
    if (session.dialog.IsOpen())
    {
        session.dialog.TakeChoice();
        chooser.Layout().Calculate();
        return std::nullopt;
    }

    std::optional<std::vector<Mii::MiiRecord>> chosen = chooser.Update();
    if (!chosen)
        return std::nullopt;

    session.saveData.SetChosenMiis(*chosen);
    session.saveFile.Save(session.saveData);
    return WiiSpeakScreenName::ChatRoom;
}

void ChooseMiisScreen::Draw()
{
    chooser.Draw();
}

std::vector<SceneLayout*> ChooseMiisScreen::ShownLayouts()
{
    return {&chooser.Layout()};
}

MiiChooser& ChooseMiisScreen::Chooser()
{
    return chooser;
}

}
