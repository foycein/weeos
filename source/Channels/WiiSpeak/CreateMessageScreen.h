#pragma once

#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/VoiceRecording.h"
#include "Channels/WiiSpeak/WiiSpeakSession.h"

#include "Sound/WavePlayer.h"

#include <optional>

namespace Channels
{

// The countdown before a recording: 3, 2, 1, a third of a second each (the animation "count" of 16000_comment has 61 frames).
constexpr double CountdownSecondsPerNumber = 20.0 / 60.0;
constexpr int CountdownStart = 3;

// Create Message (scene letter, LetterMakeScene 0x80042D44 of the program): the voice of the message with its comment window
// (16000_comment), the addressees and Help.
//
// The comment window: Begin Recording counts 3, 2, 1, then the microphone is recorded until A is pressed on the voice or 10 seconds
// are full. The voice is played back by pointing at it and pressing A; the 1 button changes the voice (Change Voice) for the next
// playback. Redo records again, OK keeps the voice in the message.
//
// WeeOS has no WiiConnect24 and so no Wii Friends: the addressees say so (10240005) and Send asks for an addressee (10240012).
// The original refuses the whole screen without Wii Friends; WeeOS opens it so that a voice can be recorded and heard, a written
// deviation (CHN-36). The photo is not built and not shown; Add Mii says that there is no Mii when the console has none.
class CreateMessageScreen : public WiiSpeakScreen
{
public:
    enum class Phase
    {
        Letter,
        CommentReady,
        CountingDown,
        Recording,
        Recorded,
    };

    explicit CreateMessageScreen(WiiSpeakSession& session);
    ~CreateMessageScreen() override;

    std::optional<WiiSpeakScreenName> Update() override;
    void Draw() override;
    std::vector<SceneLayout*> ShownLayouts() override;

    Phase CurrentPhase() const;
    const VoiceRecording& Recording() const;
    bool HasKeptVoice() const;
    bool ChangesVoice() const;
    bool IsPlayingBack();

private:
    std::optional<WiiSpeakScreenName> UpdateLetter();
    void UpdateComment();
    void PressCommentButton(const std::string& button);
    void StartRecording();
    void StopRecording();
    void PlayBack();
    void ShowCommentButtons();
    void ShowLetterVoice();

    WiiSpeakSession& session;
    SceneLayout letter;
    SceneLayout comment;
    Phase phase = Phase::Letter;
    VoiceRecording recording;
    bool isVoiceBoxShown = false;
    bool hasKeptVoice = false;
    bool changesVoice = false;
    bool isAskingToQuit = false;
    double phaseStartSeconds = 0.0;
    int shownCount = 0;
    Sound::WaveHandle playback;
    double playbackEndSeconds = 0.0;
};

}
