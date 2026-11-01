#include "Input/JoyConReports.h"

#include <algorithm>
#include <cmath>

namespace Input
{

namespace
{

constexpr std::uint8_t PacketNumberMask = 0x0F;

// The rumble data is 4 bytes for each of the two motors of a Joy-Con: high band frequency and amplitude, low band frequency and
// amplitude. "Off" is 320 Hz and 160 Hz at amplitude 0. "On" is the same frequencies at amplitude 0.5: the amplitude code
// round(log2(0.5 * 8.7) * 32) = 68 goes into the high band as 2 * 68 and into the low band as 68 / 2 + 0x40.
constexpr std::array<std::uint8_t, 8> RumbleOff = {0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40};
constexpr std::array<std::uint8_t, 8> RumbleOn = {0x00, 0x89, 0x40, 0x62, 0x00, 0x89, 0x40, 0x62};

// The places of the full report and of the answer to a subcommand.
constexpr std::size_t BatteryByte = 2;
constexpr std::size_t RightButtonByte = 3;
constexpr std::size_t SharedButtonByte = 4;
constexpr std::size_t LeftButtonByte = 5;
constexpr std::size_t LeftStickByte = 6;
constexpr std::size_t RightStickByte = 9;
constexpr std::size_t InputSize = 12;
constexpr std::size_t AcknowledgeByte = 13;
constexpr std::size_t ReplySubcommandByte = 14;
constexpr std::size_t ReplyDataByte = 15;
constexpr std::uint8_t AcknowledgedBit = 0x80;

// The full report has three samples of 12 bytes from byte 13 on: the acceleration x, y, z, then the gyroscope x, y, z, each a
// signed 16 bit number with the low byte first.
constexpr std::size_t FirstSampleByte = 13;
constexpr std::size_t SampleSize = 12;
constexpr std::size_t GyroscopeInSample = 6;
constexpr std::size_t SamplesInFullReport = 3;
constexpr std::size_t FullReportSize = FirstSampleByte + SamplesInFullReport * SampleSize;

// The battery is the high half of its byte: 8 full, 6 medium, 4 low, 2 critical, 0 empty, plus 1 while it charges.
constexpr int BatteryShift = 4;
constexpr int BatteryHalfSteps = 2;

// The answer to a read of the flash: the address (4 bytes, low byte first), the size, then the bytes.
constexpr std::size_t FlashAddressSize = 4;
constexpr std::size_t FlashSizeByte = 4;
constexpr std::size_t FlashFirstDataByte = 5;

constexpr int ByteShift = 8;
constexpr int NibbleShift = 4;
constexpr int LowNibble = 0x0F;

// Assumed (docs/audit/OpenPoints/Controllers.md, CTL-09): a Joy-Con without its calibration has the middle of its 12 bits and
// reaches 1300 to both sides; a stick within 0.15 of its middle rests.
constexpr int UsualStickMiddle = 2048;
constexpr int UsualStickRange = 1300;
constexpr float StickDeadZone = 0.15f;

// The gyroscope's formula of the notes: (raw - rest) * 936 / (reference - rest) degrees a second, with 13371 as the usual reference.
constexpr float GyroscopeReferenceDegrees = 936.0f;
constexpr int UsualGyroscopeReference = 13371;

int SignedSixteenBits(std::span<const std::uint8_t> bytes, std::size_t first)
{
    return static_cast<std::int16_t>(bytes[first] | (bytes[first + 1] << ByteShift));
}

// Three bytes hold two 12 bit values: the first in the low half, the second in the high half.
RawStick ReadRawStick(std::span<const std::uint8_t> bytes)
{
    RawStick stick;
    stick.x = bytes[0] | ((bytes[1] & LowNibble) << ByteShift);
    stick.y = (bytes[1] >> NibbleShift) | (bytes[2] << NibbleShift);
    return stick;
}

float StickAxis(int raw, int middle, int rangeAbove, int rangeBelow)
{
    int distance = raw - middle;
    int range = distance >= 0 ? rangeAbove : rangeBelow;
    if (range <= 0)
        return 0;

    return std::clamp(static_cast<float>(distance) / static_cast<float>(range), -1.0f, 1.0f);
}

void AppendLittleEndian(ReportBytes& report, std::uint32_t value)
{
    for (std::size_t byte = 0; byte < FlashAddressSize; ++byte)
        report.push_back(static_cast<std::uint8_t>(value >> (byte * ByteShift)));
}

}

ReportBytes MakeJoyConSubcommand(std::uint8_t packetNumber, bool isRumbling, std::uint8_t subcommand, std::span<const std::uint8_t> arguments)
{
    ReportBytes report = MakeJoyConRumble(packetNumber, isRumbling);
    report[0] = JoyConReportId::Subcommand;
    report.push_back(subcommand);
    report.insert(report.end(), arguments.begin(), arguments.end());
    return report;
}

ReportBytes MakeJoyConRumble(std::uint8_t packetNumber, bool isRumbling)
{
    const std::array<std::uint8_t, 8>& rumble = isRumbling ? RumbleOn : RumbleOff;
    ReportBytes report = {JoyConReportId::RumbleOnly, static_cast<std::uint8_t>(packetNumber & PacketNumberMask)};
    report.insert(report.end(), rumble.begin(), rumble.end());
    return report;
}

ReportBytes MakeJoyConFlashRead(std::uint8_t packetNumber, std::uint32_t address, std::uint8_t size)
{
    ReportBytes arguments;
    AppendLittleEndian(arguments, address);
    arguments.push_back(size);
    return MakeJoyConSubcommand(packetNumber, false, JoyConSubcommand::ReadFlash, arguments);
}

std::uint8_t JoyConPlayerLightsArgument(const PlayerLeds& leds)
{
    std::uint8_t lights = 0;
    for (int led = 0; led < PlayerLedCount; ++led)
    {
        if (leds[led])
            lights |= static_cast<std::uint8_t>(1 << led);
    }
    return lights;
}

std::optional<JoyConInput> ReadJoyConInput(std::span<const std::uint8_t> report)
{
    if (report.size() < InputSize)
        return std::nullopt;

    bool isFullReport = report[0] == JoyConReportId::FullReport;
    bool isReply = report[0] == JoyConReportId::SubcommandReply;
    if (!isFullReport && !isReply)
        return std::nullopt;

    JoyConInput input;
    input.buttons = report[RightButtonByte] | (report[SharedButtonByte] << ByteShift) | (report[LeftButtonByte] << (2 * ByteShift));
    int batteryHalfSteps = report[BatteryByte] >> BatteryShift;
    input.batteryLevel = std::min(batteryHalfSteps / BatteryHalfSteps, MaximumBatteryLevel);
    input.leftStick = ReadRawStick(report.subspan(LeftStickByte));
    input.rightStick = ReadRawStick(report.subspan(RightStickByte));

    if (!isFullReport || report.size() < FullReportSize)
        return input;

    for (std::size_t sample = 0; sample < SamplesInFullReport; ++sample)
    {
        std::size_t gyroscope = FirstSampleByte + sample * SampleSize + GyroscopeInSample;
        input.gyroscopeSamples.push_back(
            {SignedSixteenBits(report, gyroscope), SignedSixteenBits(report, gyroscope + 2), SignedSixteenBits(report, gyroscope + 4)});
    }
    return input;
}

std::optional<JoyConSubcommandReply> ReadJoyConSubcommandReply(std::span<const std::uint8_t> report)
{
    if (report.size() < ReplyDataByte || report[0] != JoyConReportId::SubcommandReply)
        return std::nullopt;

    JoyConSubcommandReply reply;
    reply.isAcknowledged = (report[AcknowledgeByte] & AcknowledgedBit) != 0;
    reply.subcommand = report[ReplySubcommandByte];
    reply.data.assign(report.begin() + ReplyDataByte, report.end());
    return reply;
}

std::optional<std::vector<std::uint8_t>> FlashBytesOfReply(const JoyConSubcommandReply& reply, std::uint32_t address)
{
    if (reply.subcommand != JoyConSubcommand::ReadFlash || !reply.isAcknowledged || reply.data.size() < FlashFirstDataByte)
        return std::nullopt;

    std::uint32_t replyAddress = 0;
    for (std::size_t byte = 0; byte < FlashAddressSize; ++byte)
        replyAddress |= static_cast<std::uint32_t>(reply.data[byte]) << (byte * ByteShift);

    std::size_t size = reply.data[FlashSizeByte];
    if (replyAddress != address || reply.data.size() < FlashFirstDataByte + size)
        return std::nullopt;

    return std::vector<std::uint8_t>(reply.data.begin() + FlashFirstDataByte, reply.data.begin() + FlashFirstDataByte + size);
}

// The left stick keeps the range above the middle, the middle, and the range below it; the right stick the middle, the range
// below and the range above.
JoyConStickCalibration ReadStickCalibration(std::span<const std::uint8_t> bytes, JoyConSide side)
{
    if (bytes.size() < JoyConFlash::StickCalibrationSize)
        return UsualStickCalibration();

    // Nine bytes hold three pairs of 12 bit values, each pair packed like a stick.
    constexpr std::size_t pairSize = 3;
    RawStick first = ReadRawStick(bytes.subspan(0));
    RawStick second = ReadRawStick(bytes.subspan(pairSize));
    RawStick third = ReadRawStick(bytes.subspan(2 * pairSize));

    if (side == JoyConSide::Left)
        return {second, first, third};

    return {first, third, second};
}

JoyConMotionCalibration ReadMotionCalibration(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < JoyConFlash::MotionCalibrationSize)
        return UsualMotionCalibration();

    // Twelve numbers: acceleration rest x, y, z, acceleration reference x, y, z, gyroscope rest x, y, z, gyroscope reference x, y, z.
    constexpr std::size_t gyroscopeRestByte = 12;
    constexpr std::size_t gyroscopeReferenceByte = 18;

    JoyConMotionCalibration calibration;
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        calibration.gyroscopeRest[axis] = SignedSixteenBits(bytes, gyroscopeRestByte + axis * 2);
        calibration.gyroscopeReference[axis] = SignedSixteenBits(bytes, gyroscopeReferenceByte + axis * 2);
    }
    return calibration;
}

JoyConStickCalibration UsualStickCalibration()
{
    RawStick middle = {UsualStickMiddle, UsualStickMiddle};
    RawStick range = {UsualStickRange, UsualStickRange};
    return {middle, range, range};
}

JoyConMotionCalibration UsualMotionCalibration()
{
    JoyConMotionCalibration calibration;
    calibration.gyroscopeReference = {UsualGyroscopeReference, UsualGyroscopeReference, UsualGyroscopeReference};
    return calibration;
}

Formats::Vector2 StickPosition(RawStick stick, const JoyConStickCalibration& calibration)
{
    float x = StickAxis(stick.x, calibration.middle.x, calibration.rangeAbove.x, calibration.rangeBelow.x);
    float y = StickAxis(stick.y, calibration.middle.y, calibration.rangeAbove.y, calibration.rangeBelow.y);

    float length = std::sqrt(x * x + y * y);
    if (length <= StickDeadZone)
        return {0, 0};

    float scaledLength = std::min(1.0f, (length - StickDeadZone) / (1.0f - StickDeadZone));
    return {x / length * scaledLength, y / length * scaledLength};
}

TurnRates TurnRatesOf(const std::array<int, 3>& sample, const JoyConMotionCalibration& calibration)
{
    std::array<float, 3> rates = {};
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        int rest = calibration.gyroscopeRest[axis];
        int range = calibration.gyroscopeReference[axis] - rest;
        if (range == 0)
            range = UsualGyroscopeReference;

        rates[axis] = static_cast<float>(sample[axis] - rest) * GyroscopeReferenceDegrees / static_cast<float>(range);
    }
    return {rates[0], rates[1], rates[2]};
}

}
