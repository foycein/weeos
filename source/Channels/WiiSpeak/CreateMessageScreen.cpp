#include "Channels/WiiSpeak/CreateMessageScreen.h"

#include "Channels/ChannelText.h"
#include "Channels/WiiSpeak/WiiSpeakMessageIds.h"

#include <array>
#include <string>

namespace Channels
{

namespace
{

// The buttons of 13100_letter.
const std::string VoiceBox = "B_nbtn_00";
const std::string AddresseesBox = "B_nbtn_02";
const std::string MiiBox = "B_nbtn_03";
const std::string SendButton = "dbutton_03";
const std::string LetterBackButton = "B_dbutton_04";
const std::string LetterHelpButton = "B_dbutton_05";

// The buttons of 16000_comment: the voice in the middle, and below it the left and the right button.
const std::string CommentVoice = "B_nbtn_00";
const std::string CommentLeftButton = "B_nbutton_01";
const std::string CommentRightButton = "B_nbutton_02";

struct PaneText
{
    std::string_view pane;
    std::uint32_t messageId;
};

constexpr std::array<PaneText, 11> LetterTexts = {{
    {"T_shadow", WiiSpeakText::CreateMessageTitle},
    {"T_micSet", WiiSpeakText::CreateMessageTitle},
    {"T_mii", WiiSpeakText::AddMii},
    {"T_Flist", WiiSpeakText::Addressees},
    {"T_photo_00", WiiSpeakText::PhotoTitle},
    {"T_photo", WiiSpeakText::AddPhotoHint},
    {"T_comment_00", WiiSpeakText::CommentTitle},
    {"T_mic", WiiSpeakText::RecordVoiceHint},
    {"T_button_03", WiiSpeakText::Send},
    {"T_button_04", WiiSpeakText::Back},
    {"T_button_05", WiiSpeakText::Help},
}};

constexpr std::uint8_t ShownAlpha = 255;
constexpr std::uint8_t HiddenAlpha = 0;
constexpr float FramesPerSecond = 60.0f;

// The sounds of the message, by their names in MicChatSound.brsar.
constexpr std::string_view FrameSound = "SE_LET_CLK_FRAME_REC";
constexpr std::string_view BackSound = "SE_LET_CLK_BTN_RET";
constexpr std::string_view HelpSound = "SE_LET_CLK_BTN_HELP";
constexpr std::string_view SendSound = "SE_LET_CLK_BTN_SEND";
constexpr std::string_view StartRecordingSound = "SE_LET_CLK_BTN_START_REC";
constexpr std::string_view EndRecordingSound = "SE_LET_CLK_BTN_END_REC";
constexpr std::string_view TimeOverSound = "SE_LET_REC_TIME_OVER";
constexpr std::string_view PlaybackSound = "SE_LET_CLK_FRAME_PREVIEW";
constexpr std::string_view RedoSound = "SE_LET_CLK_BTN_AGAIN";
constexpr std::string_view KeepSound = "SE_LET_CLK_BTN_DECIDE";
constexpr std::string_view CancelSound = "SE_LET_CLK_BTN_CANCEL_REC";
constexpr std::string_view VoiceChangeOnSound = "SE_LET_VOICE_FX_START";
constexpr std::string_view VoiceChangeOffSound = "SE_LET_VOICE_FX_END";
constexpr std::array<std::string_view, 4> CountSounds = {"SE_LET_COUNT_0", "SE_LET_COUNT_1", "SE_LET_COUNT_2", "SE_LET_COUNT_3"};

constexpr SystemMenu::ButtonSet ChangeVoiceButtons = {Input::RemoteButtons::One, 0};

}

CreateMessageScreen::CreateMessageScreen(WiiSpeakSession& messageSession, std::size_t miiCount)
    : session(messageSession)
    , letter(session.services.display, session.files.SceneLayouts("letter", "13100_letter.arc"), "13100_letter")
    , comment(session.services.display, session.files.SceneLayouts("letter", "16000_comment.arc"), "16000_comment")
    , miiCountOfTheConsole(miiCount)
{
    for (const std::string& button : {VoiceBox, AddresseesBox, MiiBox, SendButton, LetterBackButton, LetterHelpButton})
        letter.AddButton(button);

    for (const std::string& button : {CommentVoice, CommentLeftButton, CommentRightButton})
        comment.AddButton(button);

    for (const PaneText& text : LetterTexts)
        letter.SetText(text.pane, session.Text(text.messageId));

    letter.SetVisible("N_photoBox", false);
    letter.SetText("T_Cno_01", u"0");
    comment.SetText("T_comment", session.Text(WiiSpeakText::Comment));
    comment.SetText("T_change", session.Text(WiiSpeakText::CommentChangeVoice));
    comment.SetText("T_rec", session.Text(WiiSpeakText::Recording));
    letter.PlayAnimation("all_inOut");
    letter.PlayAnimation("btn_in");
}

CreateMessageScreen::~CreateMessageScreen()
{
    session.services.audio.waves.Stop(playback);
}

std::optional<WiiSpeakScreenName> CreateMessageScreen::Update()
{
    std::optional<WiiSpeakScreenName> nextScreen;
    if (phase == Phase::Letter)
        nextScreen = UpdateLetter();
    else
        UpdateComment();

    letter.Calculate();
    comment.Calculate();
    return nextScreen;
}

void CreateMessageScreen::Draw()
{
    letter.Draw();
    if (phase != Phase::Letter)
        comment.Draw();
}

std::vector<SceneLayout*> CreateMessageScreen::ShownLayouts()
{
    if (phase == Phase::Letter)
        return {&letter};

    return {&letter, &comment};
}

CreateMessageScreen::Phase CreateMessageScreen::CurrentPhase() const
{
    return phase;
}

const VoiceRecording& CreateMessageScreen::Recording() const
{
    return recording;
}

bool CreateMessageScreen::HasKeptVoice() const
{
    return hasKeptVoice;
}

bool CreateMessageScreen::ChangesVoice() const
{
    return changesVoice;
}

bool CreateMessageScreen::IsPlayingBack()
{
    return playback.IsValid() && session.services.clock.Seconds() < playbackEndSeconds;
}

std::optional<WiiSpeakScreenName> CreateMessageScreen::UpdateLetter()
{
    if (session.dialog.IsOpen() || isAskingToQuit)
    {
        std::optional<int> choice = session.dialog.TakeChoice();
        if (!isAskingToQuit || !choice)
            return std::nullopt;

        isAskingToQuit = false;
        if (*choice == 0)
            return WiiSpeakScreenName::ChatRoom;
        return std::nullopt;
    }

    std::optional<std::string> pressed = letter.TakePressedButton(session.services.input);
    if (!pressed)
        return std::nullopt;

    if (*pressed == VoiceBox)
    {
        session.services.sounds.Request(FrameSound);
        if (session.services.audio.microphone.IsMuted())
        {
            session.ShowMessage(WiiSpeakText::MutedCannotRecord, {WiiSpeakText::Ok});
            return std::nullopt;
        }

        phase = hasKeptVoice ? Phase::Recorded : Phase::CommentReady;
        comment.PlayAnimation("all_inOut");
        ShowCommentButtons();
        return std::nullopt;
    }

    if (*pressed == LetterBackButton)
    {
        session.services.sounds.Request(BackSound);
        if (!hasKeptVoice)
            return WiiSpeakScreenName::ChatRoom;

        session.ShowMessage(WiiSpeakText::QuitCreatingMessage, {WiiSpeakText::Yes, WiiSpeakText::No});
        isAskingToQuit = true;
        return std::nullopt;
    }

    if (*pressed == LetterHelpButton)
    {
        session.services.sounds.Request(HelpSound);
        session.ShowMessage(WiiSpeakText::CreateMessageHelp, {WiiSpeakText::Ok});
    }
    else if (*pressed == SendButton)
    {
        session.services.sounds.Request(SendSound);
        session.ShowMessage(WiiSpeakText::ChooseAddressees, {WiiSpeakText::Ok});
    }
    else if (*pressed == AddresseesBox)
    {
        session.services.sounds.Request(FrameSound);
        session.ShowMessage(WiiSpeakText::NoWiiFriends, {WiiSpeakText::Ok});
    }
    else if (*pressed == MiiBox && miiCountOfTheConsole == 0)
    {
        session.services.sounds.Request(FrameSound);
        session.ShowMessage(WiiSpeakText::NoMiisForMessage, {WiiSpeakText::Ok});
    }
    return std::nullopt;
}

void CreateMessageScreen::UpdateComment()
{
    double phaseSeconds = session.services.clock.Seconds() - phaseStartSeconds;
    if (phase == Phase::CountingDown)
    {
        int count = CountdownStart - int(phaseSeconds / CountdownSecondsPerNumber);
        if (count != shownCount && count >= 0)
        {
            shownCount = count;
            session.services.sounds.Request(CountSounds[count]);
            comment.SetText("T_count", AsciiToText(std::to_string(count)));
        }

        if (count <= 0)
            StartRecording();
    }

    if (phase == Phase::Recording)
    {
        recording.Add(session.heardThisStep);
        comment.ShowAnimationFrame("time_01", float(recording.Seconds()) * FramesPerSecond);
        if (recording.IsFull())
        {
            session.services.sounds.Request(TimeOverSound);
            StopRecording();
        }
    }

    if (phase == Phase::Recorded && session.services.input.AnyPlayerPressed(ChangeVoiceButtons))
    {
        changesVoice = !changesVoice;
        session.services.sounds.Request(changesVoice ? VoiceChangeOnSound : VoiceChangeOffSound);
    }

    std::optional<std::string> pressed = comment.TakePressedButton(session.services.input);
    if (pressed)
        PressCommentButton(*pressed);
}

void CreateMessageScreen::PressCommentButton(const std::string& button)
{
    if (phase == Phase::Recording && button == CommentVoice)
    {
        session.services.sounds.Request(EndRecordingSound);
        StopRecording();
        return;
    }

    if (phase == Phase::Recorded && button == CommentVoice)
    {
        PlayBack();
        return;
    }

    bool isLeftButton = button == CommentLeftButton;
    bool isRightButton = button == CommentRightButton;
    if (phase == Phase::CommentReady && isRightButton)
    {
        session.services.sounds.Request(StartRecordingSound);
        recording.Clear();
        phaseStartSeconds = session.services.clock.Seconds();
        shownCount = 0;
        comment.Layout().PaneNamed("P_circle").alpha = ShownAlpha;
        phase = Phase::CountingDown;
        ShowCommentButtons();
        return;
    }

    bool leavesTheComment = (phase == Phase::CommentReady && isLeftButton) || (phase == Phase::Recorded && isRightButton);
    if (leavesTheComment)
    {
        session.services.audio.waves.Stop(playback);
        hasKeptVoice = hasKeptVoice || (phase == Phase::Recorded && !recording.IsEmpty());
        session.services.sounds.Request(isRightButton ? KeepSound : CancelSound);
        phase = Phase::Letter;
        ShowLetterVoice();
        return;
    }

    if (phase == Phase::Recorded && isLeftButton)
    {
        session.services.sounds.Request(RedoSound);
        session.services.audio.waves.Stop(playback);
        phase = Phase::CommentReady;
        ShowCommentButtons();
    }
}

void CreateMessageScreen::StartRecording()
{
    comment.Layout().PaneNamed("P_circle").alpha = HiddenAlpha;
    comment.Layout().PaneNamed("T_rec").alpha = ShownAlpha;
    phaseStartSeconds = session.services.clock.Seconds();
    phase = Phase::Recording;
}

void CreateMessageScreen::StopRecording()
{
    comment.Layout().PaneNamed("T_rec").alpha = HiddenAlpha;
    phase = Phase::Recorded;
    ShowCommentButtons();
}

// The wave player plays on the audio thread; the screen counts the time of the playback on its own clock.
void CreateMessageScreen::PlayBack()
{
    session.services.sounds.Request(PlaybackSound);
    session.services.audio.waves.Stop(playback);
    playback = session.services.audio.waves.Play(recording.ToWave(changesVoice));
    playbackEndSeconds = session.services.clock.Seconds() + recording.Seconds();
}

void CreateMessageScreen::ShowCommentButtons()
{
    bool hasRecorded = phase == Phase::Recorded;
    comment.SetText("T_button_01", session.Text(hasRecorded ? WiiSpeakText::Redo : WiiSpeakText::Cancel));
    comment.SetText("T_button_02", session.Text(hasRecorded ? WiiSpeakText::CommentOk : WiiSpeakText::BeginRecording));
    comment.Layout().PaneNamed("P_play").alpha = hasRecorded ? ShownAlpha : HiddenAlpha;
    comment.Layout().PaneNamed("P_1").alpha = hasRecorded ? ShownAlpha : HiddenAlpha;
    comment.Layout().PaneNamed("T_change").alpha = hasRecorded ? ShownAlpha : HiddenAlpha;

    bool takesButtons = phase == Phase::CommentReady || phase == Phase::Recorded;
    comment.SetButtonEnabled(CommentLeftButton, takesButtons);
    comment.SetButtonEnabled(CommentRightButton, takesButtons);
}

void CreateMessageScreen::ShowLetterVoice()
{
    if (!hasKeptVoice)
        return;

    letter.PlayAnimation("C_on");
    letter.SetText("T_mic", u"");
}

}
