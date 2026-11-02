#pragma once

#include "Channels/WiiSpeak/PerformanceCheck.h"
#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakSession.h"

#include "Sound/WavePlayer.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace Channels
{

// The check plays its music after this wait, as the program does after 120 frames (0x801BCD58).
constexpr double CheckMusicDelaySeconds = 2.0;

// The part of the music that is measured: 10 seconds from the second second on ("your TV will emit sounds for 10 seconds",
// 4915204; the start of the part is Assumed).
constexpr double MeasureStartInMusicSeconds = 1.0;
constexpr double MeasureSeconds = 10.0;

// How late the heard music may come: the time from the speakers to the microphone and through the PC (Assumed).
constexpr double LongestEchoDelaySeconds = 0.4;

// Wii Speak Performance Check (scene echoCheck, kiuchiMicEchoCancelCheckScene 0x800A16E8 and 0x800A1B60 of the program).
// It is refused while muted or in headphone mode. Otherwise it explains itself, and Begin plays the music eccheckbgm.bns while the
// microphone listens; then it shows from 1 to 6 lights and the text of the result, or the window that the sound was too low or too
// loud to test. Try Again starts over, Finish and Cancel go back to the settings.
class PerformanceCheckScreen : public WiiSpeakScreen
{
public:
    explicit PerformanceCheckScreen(WiiSpeakSession& session);
    ~PerformanceCheckScreen() override;

    std::optional<WiiSpeakScreenName> Update() override;
    void Draw() override;
    std::vector<SceneLayout*> ShownLayouts() override;

    enum class Phase
    {
        Refused,
        Introduction,
        Ready,
        WaitingForMusic,
        Measuring,
        ShowingProblem,
        ShowingResult,
    };

    Phase CurrentPhase() const;
    std::optional<CheckResult> LastResult() const;

private:
    void ShowReady();
    void Begin();
    void FollowTheMeasurement();
    void Judge();
    std::optional<WiiSpeakScreenName> FollowTheWindow();
    std::optional<WiiSpeakScreenName> PressButton(const std::string& button);

    WiiSpeakSession& session;
    SceneLayout layout;
    std::shared_ptr<const Sound::DecodedWave> music;
    std::vector<std::int16_t> playedPart;
    std::vector<std::int16_t> heardPart;
    Sound::WaveHandle musicHandle;
    Phase phase = Phase::Introduction;
    double phaseStartSeconds = 0.0;
    std::optional<CheckResult> result;
};

}
