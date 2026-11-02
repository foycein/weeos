#include "Input/JoyCon.h"

#include <cmath>

namespace Input
{

namespace
{

// Assumed: a Joy-Con sends 60 reports in a second; after 2 seconds of silence it is
// not connected and is set up again.
constexpr double SilenceBeforeLost = 2.0;

constexpr std::uint8_t FullReportMode = 0x30;
constexpr int PacketNumberCount = 16;
constexpr std::uint8_t SwitchOn = 0x01;

// Assumed: a turn slower than this is the drift of a gyroscope at rest, not the hand.
constexpr float GyroscopeRestingRate = 1.5f;

// The bits of the buttons that belong to each side; the shared byte holds both sides' buttons, each side reports its own.
constexpr std::uint32_t RightSideButtons = 0x00FFFF;
constexpr std::uint32_t LeftSideButtons = 0xFFFF00;

float WithoutDrift(float rate)
{
    return std::abs(rate) < GyroscopeRestingRate ? 0.0f : rate;
}

}

JoyCon::JoyCon(std::unique_ptr<Platform::HidConnection> hidConnection, JoyConSide joyConSide)
    : connection(std::move(hidConnection))
    , side(joyConSide)
{
    SetUp();
}

void JoyCon::Poll(double nowInSeconds)
{
    now = nowInSeconds;

    while (std::optional<ReportBytes> report = connection->TakeReport())
        HandleReport(*report);

    bool isSilent = !hasReport || now - lastReportTime > SilenceBeforeLost;
    if (isSilent && now - lastSetUpTime > SilenceBeforeLost)
        SetUp();

    if (waitingReports.empty())
        return;

    Send(waitingReports.front());
    waitingReports.pop_front();
}

bool JoyCon::IsPresent() const
{
    return connection->IsOpen();
}

bool JoyCon::IsConnected() const
{
    return hasReport && now - lastReportTime <= SilenceBeforeLost;
}

JoyConSide JoyCon::Side() const
{
    return side;
}

std::uint32_t JoyCon::Buttons() const
{
    if (!IsConnected())
        return 0;

    return buttons;
}

Formats::Vector2 JoyCon::Stick() const
{
    if (!IsConnected())
        return {0, 0};

    return StickPosition(rawStick, stickCalibration);
}

int JoyCon::BatteryLevel() const
{
    return batteryLevel;
}

bool JoyCon::TakeNewData()
{
    bool hadNewData = hasNewData;
    hasNewData = false;
    return hadNewData;
}

Formats::Vector3 JoyCon::TakeTurn()
{
    Formats::Vector3 taken = turn;
    turn = {0, 0, 0};
    return taken;
}

void JoyCon::SetRumble(bool shouldRumble)
{
    if (shouldRumble == isRumbling)
        return;

    isRumbling = shouldRumble;
    Send(MakeJoyConRumble(0, isRumbling));
}

void JoyCon::SetPlayerLeds(const PlayerLeds& leds)
{
    playerLeds = leds;
    std::array<std::uint8_t, 1> lights = {JoyConPlayerLightsArgument(playerLeds)};
    Send(MakeJoyConSubcommand(0, isRumbling, JoyConSubcommand::SetPlayerLights, lights));
}

void JoyCon::HandleReport(const ReportBytes& report)
{
    if (std::optional<JoyConSubcommandReply> reply = ReadJoyConSubcommandReply(report))
        HandleReply(*reply);

    std::optional<JoyConInput> input = ReadJoyConInput(report);
    if (!input)
        return;

    hasReport = true;
    lastReportTime = now;
    hasNewData = true;
    batteryLevel = input->batteryLevel;

    std::uint32_t ownButtons = side == JoyConSide::Right ? RightSideButtons : LeftSideButtons;
    buttons = input->buttons & ownButtons;
    rawStick = side == JoyConSide::Right ? input->rightStick : input->leftStick;

    for (const std::array<int, 3>& sample : input->gyroscopeSamples)
        AddTurn(sample);
}

void JoyCon::HandleReply(const JoyConSubcommandReply& reply)
{
    std::uint32_t stickAddress = side == JoyConSide::Right ? JoyConFlash::RightStickCalibration : JoyConFlash::LeftStickCalibration;
    if (std::optional<std::vector<std::uint8_t>> stick = FlashBytesOfReply(reply, stickAddress))
        stickCalibration = ReadStickCalibration(*stick, side);

    if (std::optional<std::vector<std::uint8_t>> motion = FlashBytesOfReply(reply, JoyConFlash::MotionCalibration))
        motionCalibration = ReadMotionCalibration(*motion);
}

void JoyCon::AddTurn(const std::array<int, 3>& sample)
{
    TurnRates rates = TurnRatesOf(sample, motionCalibration);
    float seconds = static_cast<float>(JoyConSampleSeconds);
    turn.x += WithoutDrift(rates.x) * seconds;
    turn.y += WithoutDrift(rates.y) * seconds;
    turn.z += WithoutDrift(rates.z) * seconds;
}

// One subcommand in every step: the motor and the gyroscope on, the two calibrations, the full report, the lights.
void JoyCon::SetUp()
{
    lastSetUpTime = now;
    waitingReports.clear();

    std::array<std::uint8_t, 1> switchOn = {SwitchOn};
    std::array<std::uint8_t, 1> fullReport = {FullReportMode};
    std::array<std::uint8_t, 1> lights = {JoyConPlayerLightsArgument(playerLeds)};
    std::uint32_t stickAddress = side == JoyConSide::Right ? JoyConFlash::RightStickCalibration : JoyConFlash::LeftStickCalibration;

    waitingReports.push_back(MakeJoyConSubcommand(0, false, JoyConSubcommand::EnableVibration, switchOn));
    waitingReports.push_back(MakeJoyConSubcommand(0, false, JoyConSubcommand::EnableMotion, switchOn));
    waitingReports.push_back(MakeJoyConFlashRead(0, stickAddress, JoyConFlash::StickCalibrationSize));
    waitingReports.push_back(MakeJoyConFlashRead(0, JoyConFlash::MotionCalibration, JoyConFlash::MotionCalibrationSize));
    waitingReports.push_back(MakeJoyConSubcommand(0, false, JoyConSubcommand::SetReportMode, fullReport));
    waitingReports.push_back(MakeJoyConSubcommand(0, false, JoyConSubcommand::SetPlayerLights, lights));
}

// The packet number of every report counts up; the reports that wait get theirs when they go out.
void JoyCon::Send(ReportBytes report)
{
    report[1] = NextPacketNumber();
    connection->SendReport(report);
}

std::uint8_t JoyCon::NextPacketNumber()
{
    std::uint8_t number = packetNumber;
    packetNumber = static_cast<std::uint8_t>((packetNumber + 1) % PacketNumberCount);
    return number;
}

}
