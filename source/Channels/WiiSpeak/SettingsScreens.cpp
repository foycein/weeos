#include "Channels/WiiSpeak/SettingsScreens.h"

#include "Channels/ChannelText.h"
#include "Channels/WiiSpeak/MicrophoneLevel.h"
#include "Channels/WiiSpeak/WiiSpeakMessageIds.h"

#include <array>
#include <string>

namespace Channels
{

namespace
{

constexpr SystemMenu::ButtonSet PlusButtons = {Input::RemoteButtons::Plus, Input::ClassicButtons::Plus};
constexpr SystemMenu::ButtonSet MinusButtons = {Input::RemoteButtons::Minus, Input::ClassicButtons::Minus};

// The sounds of the settings, by their names in MicChatSound.brsar.
constexpr std::string_view ButtonSound = "SE_CON_CLK_BTN";
constexpr std::string_view OkSound = "SE_CON_CLK_BTN_OK";
constexpr std::string_view BackSound = "SE_CON_CLK_BTN_CANCEL";
constexpr std::string_view GainUpSound = "SE_CON_CLK_GAIN_INC";
constexpr std::string_view GainDownSound = "SE_CON_CLK_GAIN_DEC";
constexpr std::string_view HeadphonesOnSound = "SE_CON_CLK_BTN_PHONE_ON";
constexpr std::string_view HeadphonesOffSound = "SE_CON_CLK_BTN_PHONE_OFF";

// ---- The settings menu (19010_setMenu).

const std::string SensitivityButton = "nbutton_00";
const std::string HeadphoneButton = "nbutton_01";
const std::string CheckButton = "nbutton_02";
const std::string PrecautionsButton = "nbutton_03";
const std::string MenuBackButton = "B_dbutton_04";
const std::string MenuHelpButton = "B_dbutton_05";

constexpr std::array<PaneText, 8> MenuTexts = {{
    {"T_shadow", WiiSpeakText::SettingsTitle},
    {"T_micSet", WiiSpeakText::SettingsTitle},
    {"T_button_00", WiiSpeakText::SettingsSensitivity},
    {"T_button_01", WiiSpeakText::SettingsHeadphones},
    {"T_button_02", WiiSpeakText::SettingsCheck},
    {"T_button_03", WiiSpeakText::SettingsPrecautions},
    {"T_button_04", WiiSpeakText::SettingsBack},
    {"T_button_05", WiiSpeakText::SettingsHelp},
}};

// ---- The sensitivity (19000_micSet).

const std::string DownButton = "nbtn_00";
const std::string UpButton = "nbtn_01";
const std::string SensitivityOkButton = "dbutton_02";

constexpr std::array<PaneText, 7> SensitivityTexts = {{
    {"T_shadow", WiiSpeakText::SensitivityTitle},
    {"T_micSet", WiiSpeakText::SensitivityTitle},
    {"T_howto", WiiSpeakText::SensitivityHowTo},
    {"T_L", WiiSpeakText::High},
    {"T_S", WiiSpeakText::Low},
    {"T_OK", WiiSpeakText::OkZone},
    {"T_button_02", WiiSpeakText::SensitivityOk},
}};

// ---- The headphones (19100_hpSet).

const std::string YesButton = "nbutton_00";
const std::string NoButton = "nbutton_01";
const std::string HeadphoneOkButton = "dbutton_02";
constexpr std::uint8_t ChosenFrameAlpha = 255;
constexpr std::uint8_t OtherFrameAlpha = 0;

std::uint8_t FrameAlpha(bool isChosen)
{
    if (isChosen)
        return ChosenFrameAlpha;

    return OtherFrameAlpha;
}

constexpr std::array<PaneText, 6> HeadphoneTexts = {{
    {"T_shadow", WiiSpeakText::HeadphoneTitle},
    {"T_micSet", WiiSpeakText::HeadphoneTitle},
    {"T_howto_00", WiiSpeakText::HeadphoneQuestion},
    {"T_button_00", WiiSpeakText::HeadphoneYes},
    {"T_button_01", WiiSpeakText::HeadphoneNo},
    {"T_button_02", WiiSpeakText::HeadphoneOk},
}};

// ---- The precautions (20001_attention).

constexpr int PageCount = 8;
constexpr std::uint32_t PrecautionsLayout = 20001;
const std::string PageBackButton = "dbutton_01";
const std::string PageNextButton = "dbutton_02";

constexpr std::array<PaneText, 33> PrecautionTexts = {{
    {"T_texHdP1_00", WiiSpeakMessageId(PrecautionsLayout, 3)},
    {"T_texP1", WiiSpeakMessageId(PrecautionsLayout, 4)},
    {"T_texHdP2_00", WiiSpeakMessageId(PrecautionsLayout, 5)},
    {"T_texP2_00", WiiSpeakMessageId(PrecautionsLayout, 7)},
    {"T_texP2_01", WiiSpeakMessageId(PrecautionsLayout, 8)},
    {"T_texP2_02", WiiSpeakMessageId(PrecautionsLayout, 9)},
    {"T_texHdP3_00", WiiSpeakMessageId(PrecautionsLayout, 10)},
    {"T_texP3_00", WiiSpeakMessageId(PrecautionsLayout, 12)},
    {"T_texHdP4_00", WiiSpeakMessageId(PrecautionsLayout, 13)},
    {"T_texP4_00", WiiSpeakMessageId(PrecautionsLayout, 14)},
    {"T_textMiniP4_00", WiiSpeakMessageId(PrecautionsLayout, 35)},
    {"T_textMiniP4_01", WiiSpeakMessageId(PrecautionsLayout, 36)},
    {"T_textMiniP4_02", WiiSpeakMessageId(PrecautionsLayout, 37)},
    {"T_textMiniP4_03", WiiSpeakMessageId(PrecautionsLayout, 38)},
    {"T_textMiniP4_04", WiiSpeakMessageId(PrecautionsLayout, 39)},
    {"T_textMiniP4_05", WiiSpeakMessageId(PrecautionsLayout, 40)},
    {"T_texHdP5_00", WiiSpeakMessageId(PrecautionsLayout, 15)},
    {"T_textP5_00", WiiSpeakMessageId(PrecautionsLayout, 16)},
    {"T_textMiniP5_01", WiiSpeakMessageId(PrecautionsLayout, 18)},
    {"T_textMiniP5_02", WiiSpeakMessageId(PrecautionsLayout, 19)},
    {"T_textMiniP5_04", WiiSpeakMessageId(PrecautionsLayout, 21)},
    {"T_textMiniP5_06", WiiSpeakMessageId(PrecautionsLayout, 23)},
    {"T_texHdP6_00", WiiSpeakMessageId(PrecautionsLayout, 25)},
    {"T_textP6_00", WiiSpeakMessageId(PrecautionsLayout, 26)},
    {"T_textMiniP6_00", WiiSpeakMessageId(PrecautionsLayout, 27)},
    {"T_texHdP7_00", WiiSpeakMessageId(PrecautionsLayout, 28)},
    {"T_texP7_00", WiiSpeakMessageId(PrecautionsLayout, 29)},
    {"T_textMiniP7_01", WiiSpeakMessageId(PrecautionsLayout, 42)},
    {"T_textMiniP7_02", WiiSpeakMessageId(PrecautionsLayout, 43)},
    {"T_textMiniP7_03", WiiSpeakMessageId(PrecautionsLayout, 44)},
    {"T_textMiniP7_04", WiiSpeakMessageId(PrecautionsLayout, 45)},
    {"T_texHdP8_00", WiiSpeakMessageId(PrecautionsLayout, 30)},
    {"T_texP8_00", WiiSpeakMessageId(PrecautionsLayout, 31)},
}};

}

SettingsMenuScreen::SettingsMenuScreen(WiiSpeakSession& menuSession)
    : session(menuSession)
    , water(WiiSpeakWater::OfTheMenus(menuSession, WiiSpeakWater::MenuColor::Settings))
    , layout(session.services.display, session.files.SceneLayouts("setMenu", "19010_setMenu.arc"), "19010_setMenu")
{
    for (const std::string& button : {SensitivityButton, HeadphoneButton, CheckButton, PrecautionsButton, MenuBackButton, MenuHelpButton})
        layout.AddButton(button);

    session.SetTexts(layout, MenuTexts);
    layout.PlayAnimation("all_inOut");
    layout.PlayAnimation("btn_in");
}

std::optional<WiiSpeakScreenName> SettingsMenuScreen::Update()
{
    water->Update();
    std::optional<WiiSpeakScreenName> nextScreen;
    if (session.dialog.IsOpen())
    {
        session.dialog.TakeChoice();
        layout.Calculate();
        return nextScreen;
    }

    std::optional<std::string> pressed = layout.TakePressedButton(session.services.input);
    if (pressed == SensitivityButton)
        nextScreen = WiiSpeakScreenName::Sensitivity;
    else if (pressed == HeadphoneButton)
        nextScreen = WiiSpeakScreenName::Headphones;
    else if (pressed == CheckButton)
        nextScreen = WiiSpeakScreenName::PerformanceCheck;
    else if (pressed == PrecautionsButton)
        nextScreen = WiiSpeakScreenName::Precautions;
    else if (pressed == MenuBackButton)
        nextScreen = WiiSpeakScreenName::ChatRoom;
    else if (pressed == MenuHelpButton)
        session.ShowMessage(WiiSpeakText::SettingsHelpText, {WiiSpeakText::Ok});

    if (pressed)
        session.services.sounds.Request(pressed == MenuBackButton ? BackSound : ButtonSound);

    layout.Calculate();
    return nextScreen;
}

void SettingsMenuScreen::Draw()
{
    water->Draw();
    session.ripples.Draw();
    layout.Draw();
}

std::vector<SceneLayout*> SettingsMenuScreen::ShownLayouts()
{
    return {&layout};
}

SensitivityScreen::SensitivityScreen(WiiSpeakSession& sensitivitySession)
    : session(sensitivitySession)
    , layout(session.services.display, session.files.SceneLayouts("micSet", "19000_micSet.arc"), "19000_micSet")
    , sensitivity(session.saveData.Sensitivity())
{
    for (const std::string& button : {DownButton, UpButton, SensitivityOkButton})
        layout.AddButton(button);

    session.SetTexts(layout, SensitivityTexts);
    layout.PlayAnimation("all_inOut");
    ShowSensitivity();
}

std::optional<WiiSpeakScreenName> SensitivityScreen::Update()
{
    meterFrame = MeterFrame(HeardLevelInDecibels(session.heardThisStep));
    layout.ShowAnimationFrame("mic_level", meterFrame);

    std::optional<std::string> pressed = layout.TakePressedButton(session.services.input);
    if (pressed == DownButton || session.services.input.AnyPlayerPressed(MinusButtons))
        ChangeSensitivity(-1);

    if (pressed == UpButton || session.services.input.AnyPlayerPressed(PlusButtons))
        ChangeSensitivity(+1);

    layout.Calculate();
    if (pressed != SensitivityOkButton)
        return std::nullopt;

    session.services.sounds.Request(OkSound);
    session.saveData.SetSensitivity(sensitivity);
    session.saveFile.Save(session.saveData);
    return WiiSpeakScreenName::SettingsMenu;
}

void SensitivityScreen::Draw()
{
    layout.Draw();
}

std::vector<SceneLayout*> SensitivityScreen::ShownLayouts()
{
    return {&layout};
}

int SensitivityScreen::ShownSensitivity() const
{
    return sensitivity;
}

float SensitivityScreen::ShownMeterFrame() const
{
    return meterFrame;
}

// The arrows go no further than 1 and 5; the arrow that cannot be used is hidden, as in 0x80091A54.
void SensitivityScreen::ChangeSensitivity(int step)
{
    int changed = sensitivity + step;
    if (changed < Ios::LowestMicrophoneSensitivity || changed > Ios::HighestMicrophoneSensitivity)
        return;

    sensitivity = changed;
    session.services.audio.microphone.SetSensitivity(sensitivity);
    session.services.sounds.Request(step > 0 ? GainUpSound : GainDownSound);
    ShowSensitivity();
}

void SensitivityScreen::ShowSensitivity()
{
    layout.SetText("T_Sno", AsciiToText(std::to_string(sensitivity)));
    layout.ShowAnimationFrame("lvBox", float(sensitivity - Ios::LowestMicrophoneSensitivity));
    layout.SetVisible("P_down", sensitivity > Ios::LowestMicrophoneSensitivity);
    layout.SetVisible("P_up", sensitivity < Ios::HighestMicrophoneSensitivity);
}

HeadphoneScreen::HeadphoneScreen(WiiSpeakSession& headphoneSession)
    : session(headphoneSession)
    , layout(session.services.display, session.files.SceneLayouts("hpSet", "19100_hpSet.arc"), "19100_hpSet")
    , choosesHeadphones(session.usesHeadphones)
{
    for (const std::string& button : {YesButton, NoButton, HeadphoneOkButton})
        layout.AddButton(button);

    session.SetTexts(layout, HeadphoneTexts);
    layout.PlayAnimation("all_inOut");
    ShowChoice();
}

std::optional<WiiSpeakScreenName> HeadphoneScreen::Update()
{
    if (isTellingTheMode)
    {
        layout.Calculate();
        if (!session.dialog.TakeChoice())
            return std::nullopt;

        return WiiSpeakScreenName::SettingsMenu;
    }

    std::optional<std::string> pressed = layout.TakePressedButton(session.services.input);
    if (pressed == YesButton || pressed == NoButton)
    {
        choosesHeadphones = pressed == YesButton;
        session.services.sounds.Request(choosesHeadphones ? HeadphonesOnSound : HeadphonesOffSound);
        ShowChoice();
    }

    if (pressed == HeadphoneOkButton)
    {
        session.services.sounds.Request(OkSound);
        session.usesHeadphones = choosesHeadphones;
        session.ShowMessage(choosesHeadphones ? WiiSpeakText::HeadphonesOn : WiiSpeakText::HeadphonesOff, {WiiSpeakText::Ok});
        isTellingTheMode = true;
    }

    layout.Calculate();
    return std::nullopt;
}

void HeadphoneScreen::Draw()
{
    layout.Draw();
}

std::vector<SceneLayout*> HeadphoneScreen::ShownLayouts()
{
    return {&layout};
}

// The frame around the chosen answer: W_box_00 around Yes, W_box_01 around No.
void HeadphoneScreen::ShowChoice()
{
    layout.Layout().PaneNamed("W_box_00").alpha = FrameAlpha(choosesHeadphones);
    layout.Layout().PaneNamed("W_box_01").alpha = FrameAlpha(!choosesHeadphones);
}

PrecautionsScreen::PrecautionsScreen(WiiSpeakSession& precautionsSession, WiiSpeakScreenName screenAfterwards)
    : session(precautionsSession)
    , layout(session.services.display, session.files.SceneLayouts("attention", "20001_attention.arc"), "20001_attention")
    , nextScreen(screenAfterwards)
{
    layout.AddButton(PageBackButton);
    layout.AddButton(PageNextButton);
    session.SetTexts(layout, PrecautionTexts);
    layout.SetText("T_button_01", session.Text(WiiSpeakText::PrecautionsBack));
    layout.SetText("T_Pno_03", AsciiToText(std::to_string(PageCount)));
    layout.PlayAnimation("dialog_in");
    ShowPage();
}

std::optional<WiiSpeakScreenName> PrecautionsScreen::Update()
{
    std::optional<std::string> pressed = layout.TakePressedButton(session.services.input);
    layout.Calculate();
    if (!pressed)
        return std::nullopt;

    session.services.sounds.Request(ButtonSound);
    int newPage = pressed == PageNextButton ? page + 1 : page - 1;
    if (newPage < 1 || newPage > PageCount)
        return nextScreen;

    page = newPage;
    layout.PlayAnimation("page_turn");
    ShowPage();
    return std::nullopt;
}

void PrecautionsScreen::Draw()
{
    layout.Draw();
}

std::vector<SceneLayout*> PrecautionsScreen::ShownLayouts()
{
    return {&layout};
}

int PrecautionsScreen::ShownPage() const
{
    return page;
}

void PrecautionsScreen::ShowPage()
{
    for (int pageNumber = 1; pageNumber <= PageCount; ++pageNumber)
        layout.SetVisible("N_page" + std::to_string(pageNumber), pageNumber == page);

    layout.SetText("T_Pno_01", AsciiToText(std::to_string(page)));
    bool isLastPage = page == PageCount;
    layout.SetText("T_button_02", session.Text(isLastPage ? WiiSpeakText::PrecautionsClose : WiiSpeakText::PrecautionsNext));
}

}
