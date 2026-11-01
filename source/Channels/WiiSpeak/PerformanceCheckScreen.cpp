#include "Channels/WiiSpeak/PerformanceCheckScreen.h"

#include "Channels/WiiSpeak/WiiSpeakMessageIds.h"

#include "Ios/Microphone/MicrophoneService.h"
#include "Sound/StreamFile.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Channels
{

namespace
{

const std::string LeftButton = "B_dbutton_01";
const std::string RightButton = "B_dbutton_02";
const std::string LevelAnimation = "mic_level";

constexpr float NoLightsFrame = 0.0f;
constexpr double FullScale = 32767.0;
constexpr int BestScoreOfABadResult = 1;
constexpr int BestScoreOfAMiddleResult = 3;

// The sounds of the check, by their names in MicChatSound.brsar (0x5E to 0x60 of 0x800A1B60).
constexpr std::string_view ButtonSound = "SE_CON_CLK_BTN";
constexpr std::string_view GoodResultSound = "SE_CON_EC_RESULT_GOOD";
constexpr std::string_view MiddleResultSound = "SE_CON_EC_RESULT_OK";
constexpr std::string_view BadResultSound = "SE_CON_EC_RESULT_BAD";

// The part of the music that is measured, as the microphone would hear it: both sides together, at the rate of the microphone.
std::vector<std::int16_t> MeasuredPartOfTheMusic(const Sound::DecodedWave& music)
{
    std::vector<std::int16_t> part;
    std::size_t sampleCount = std::size_t(MeasureSeconds * Ios::MicrophoneSampleRate);
    std::size_t firstMicrophoneSample = std::size_t(MeasureStartInMusicSeconds * Ios::MicrophoneSampleRate);
    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        // Whole numbers, so that every microphone sample takes the same sample of the music's own rate.
        std::size_t microphoneSample = firstMicrophoneSample + sample;
        std::size_t musicSample = std::min(microphoneSample * music.sampleRate / Ios::MicrophoneSampleRate, music.SampleCount() - 1);
        double sum = 0.0;
        for (const std::vector<float>& channel : music.channels)
            sum += channel[musicSample];

        double mono = sum / double(music.channels.size());
        part.push_back(static_cast<std::int16_t>(std::round(mono * FullScale)));
    }
    return part;
}

}

PerformanceCheckScreen::PerformanceCheckScreen(WiiSpeakSession& checkSession)
    : session(checkSession)
    , layout(session.services.display, session.files.SceneLayouts("echoCheck", "19200_check.arc"), "19200_check")
    , music(std::make_shared<Sound::DecodedWave>(Sound::ReadStreamFile(session.files.CheckMusic())))
    , playedPart(MeasuredPartOfTheMusic(*music))
{
    layout.AddButton(LeftButton);
    layout.AddButton(RightButton);
    layout.SetText("T_shadow", session.Text(WiiSpeakText::CheckTitle));
    layout.SetText("T_micSet", session.Text(WiiSpeakText::CheckTitle));
    layout.SetText("T_howto_00", session.Text(WiiSpeakText::CheckHowTo));
    layout.SetText("T_howto_01", session.Text(WiiSpeakText::CheckNotFunctioning));
    layout.SetText("T_howto_02", session.Text(WiiSpeakText::CheckFunctioningFineTune));
    layout.SetText("T_howto_03", session.Text(WiiSpeakText::CheckFunctioning));
    layout.PlayAnimation("all_inOut");
    ShowReady();

    if (session.services.audio.microphone.IsMuted())
    {
        session.ShowMessage(WiiSpeakText::CheckRefusedMuted, {WiiSpeakText::Ok});
        phase = Phase::Refused;
        return;
    }

    if (session.usesHeadphones)
    {
        session.ShowMessage(WiiSpeakText::CheckRefusedHeadphones, {WiiSpeakText::Ok});
        phase = Phase::Refused;
        return;
    }

    session.ShowMessage(WiiSpeakText::CheckIntroduction, {WiiSpeakText::Ok});
}

PerformanceCheckScreen::~PerformanceCheckScreen()
{
    session.services.audio.waves.Stop(musicHandle);
}

std::optional<WiiSpeakScreenName> PerformanceCheckScreen::Update()
{
    std::optional<WiiSpeakScreenName> nextScreen;
    if (phase == Phase::Refused && session.dialog.TakeChoice())
        nextScreen = WiiSpeakScreenName::SettingsMenu;

    if (phase == Phase::Introduction && session.dialog.TakeChoice())
        phase = Phase::Ready;

    if (phase == Phase::ShowingProblem && session.dialog.TakeChoice())
        ShowReady();

    if (phase == Phase::WaitingForMusic || phase == Phase::Measuring)
        FollowTheMeasurement();

    bool takesButtons = phase == Phase::Ready || phase == Phase::ShowingResult;
    std::optional<std::string> pressed = layout.TakePressedButton(session.services.input);
    if (takesButtons && pressed)
        nextScreen = PressButton(*pressed);

    layout.Calculate();
    return nextScreen;
}

void PerformanceCheckScreen::Draw()
{
    layout.Draw();
}

std::vector<SceneLayout*> PerformanceCheckScreen::ShownLayouts()
{
    return {&layout};
}

PerformanceCheckScreen::Phase PerformanceCheckScreen::CurrentPhase() const
{
    return phase;
}

std::optional<CheckResult> PerformanceCheckScreen::LastResult() const
{
    return result;
}

void PerformanceCheckScreen::ShowReady()
{
    layout.SetText("T_button_01", session.Text(WiiSpeakText::CheckCancel));
    layout.SetText("T_button_02", session.Text(WiiSpeakText::CheckBegin));
    layout.ShowAnimationFrame(LevelAnimation, NoLightsFrame);
    phase = Phase::Ready;
}

std::optional<WiiSpeakScreenName> PerformanceCheckScreen::PressButton(const std::string& button)
{
    session.services.sounds.Request(ButtonSound);
    bool leaves = phase == Phase::Ready ? button == LeftButton : button == RightButton;
    if (leaves)
        return WiiSpeakScreenName::SettingsMenu;

    Begin();
    return std::nullopt;
}

void PerformanceCheckScreen::Begin()
{
    heardPart.clear();
    result.reset();
    layout.ShowAnimationFrame(LevelAnimation, NoLightsFrame);
    session.ShowMessage(WiiSpeakText::CheckRunning, {});
    phaseStartSeconds = session.services.clock.Seconds();
    phase = Phase::WaitingForMusic;
}

// The music starts after its wait; the microphone is kept for the measured part; the result comes when the music has ended.
void PerformanceCheckScreen::FollowTheMeasurement()
{
    double phaseSeconds = session.services.clock.Seconds() - phaseStartSeconds;
    if (phase == Phase::WaitingForMusic)
    {
        if (phaseSeconds < CheckMusicDelaySeconds)
            return;

        musicHandle = session.services.audio.waves.Play(music);
        phaseStartSeconds = session.services.clock.Seconds();
        phase = Phase::Measuring;
        return;
    }

    bool isInMeasuredPart = phaseSeconds >= MeasureStartInMusicSeconds && heardPart.size() < playedPart.size();
    if (isInMeasuredPart)
        heardPart.insert(heardPart.end(), session.heardThisStep.begin(), session.heardThisStep.end());

    double musicSeconds = double(music->SampleCount()) / double(music->sampleRate);
    if (phaseSeconds < musicSeconds)
        return;

    session.dialog.Close();
    Judge();
}

void PerformanceCheckScreen::Judge()
{
    std::size_t delaySamples = std::size_t(LongestEchoDelaySeconds * Ios::MicrophoneSampleRate);
    heardPart.resize(std::min(heardPart.size(), playedPart.size()));
    std::vector<std::int16_t> reduced = RemoveEcho(heardPart, playedPart, delaySamples);
    result = JudgeLevels(LevelOf(heardPart), LevelOf(reduced));

    if (result == CheckResult::TooQuiet || result == CheckResult::TooLoud)
    {
        session.services.sounds.Request(BadResultSound);
        session.ShowMessage(result == CheckResult::TooQuiet ? WiiSpeakText::CheckTooQuiet : WiiSpeakText::CheckTooLoud, {WiiSpeakText::Ok});
        phase = Phase::ShowingProblem;
        return;
    }

    int score = static_cast<int>(*result);
    if (score <= BestScoreOfABadResult)
        session.services.sounds.Request(BadResultSound);
    else if (score <= BestScoreOfAMiddleResult)
        session.services.sounds.Request(MiddleResultSound);
    else
        session.services.sounds.Request(GoodResultSound);

    layout.ShowAnimationFrame(LevelAnimation, float(score + 1));
    layout.SetText("T_button_01", session.Text(WiiSpeakText::CheckTryAgain));
    layout.SetText("T_button_02", session.Text(WiiSpeakText::CheckFinish));
    phase = Phase::ShowingResult;
}

}
