#include "Input/WiiRemoteReports.h"

#include <algorithm>
#include <cmath>

namespace Input
{

namespace
{

// The byte after the number of a report sent to the remote.
constexpr std::uint8_t RumbleBit = 0x01;
constexpr std::uint8_t SwitchOnBit = 0x04;
constexpr std::uint8_t ContinuousBit = 0x04;
constexpr std::uint8_t RegisterSpaceBit = 0x04;

// The four lights are the high four bits of the report PlayerLeds, light 1 first.
constexpr std::uint8_t FirstLedBit = 0x10;

// Addresses from this one on are registers, the ones below are the memory of the remote.
constexpr std::uint32_t FirstRegisterAddress = 0xA00000;

// A write carries at most 16 bytes, a speaker packet at most 20; the size of the speaker packet is written three bits up.
constexpr std::size_t LargestWrite = 16;
constexpr std::size_t LargestSpeakerPacket = 20;
constexpr int SpeakerSizeShift = 3;

// The buttons are the low 13 bits of the first byte and the second byte, except the bits that carry the low bits of the acceleration.
constexpr ButtonMask AllRemoteButtons = 0x9F1F;

// The position of things in the reports from the remote.
constexpr std::size_t FirstButtonByte = 1;
constexpr std::size_t SecondButtonByte = 2;
constexpr std::size_t StatusFlagsByte = 3;
constexpr std::size_t StatusBatteryByte = 6;
constexpr std::size_t StatusReportSize = 7;
constexpr std::size_t MemorySizeAndErrorByte = 3;
constexpr std::size_t MemoryAddressHighByte = 4;
constexpr std::size_t MemoryAddressLowByte = 5;
constexpr std::size_t MemoryFirstDataByte = 6;
constexpr std::size_t MemoryDataReportSize = 22;
constexpr std::size_t AccelerationXByte = 3;
constexpr std::size_t AccelerationYByte = 4;
constexpr std::size_t AccelerationZByte = 5;
constexpr std::size_t FirstDotGroupByte = 6;
constexpr std::size_t SecondDotGroupByte = 11;
constexpr std::size_t ExtensionFirstByte = 16;
constexpr std::size_t DataReportSize = 22;

constexpr std::uint8_t StatusHasExtension = 0x02;
constexpr std::uint8_t StatusSpeakerOn = 0x04;
constexpr std::uint8_t StatusCameraOn = 0x08;

constexpr int TwoBits = 0x03;
constexpr int LowNibble = 0x0F;
constexpr int NibbleShift = 4;
constexpr int ByteShift = 8;

// The camera takes dots of up to 10 bits; a place that holds no dot is all ones. Its picture is 768 pixels high.
constexpr float CameraPictureHeight = 768;

// The battery byte and its level: below 3 is level 0, below 51 level 1, below 68 level 2, below 85 level 3.
constexpr auto BatteryLevelLimits = std::to_array<int>({3, 51, 68, 85});

// The sensitivity blocks of the camera for the five levels of the console (WiiBrew, IR Camera, "Sensitivity Settings").
constexpr int FirstSensitivityBlockSize = 9;
constexpr int SecondSensitivityBlockSize = 2;

struct SensitivityBlocks
{
    std::array<std::uint8_t, FirstSensitivityBlockSize> first;
    std::array<std::uint8_t, SecondSensitivityBlockSize> second;
};

constexpr auto SensitivityLevels = std::to_array<SensitivityBlocks>({
    {{0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0x64, 0x00, 0xFE}, {0xFD, 0x05}},
    {{0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0x96, 0x00, 0xB4}, {0xB3, 0x04}},
    {{0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xAA, 0x00, 0x64}, {0x63, 0x03}},
    {{0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xC8, 0x00, 0x36}, {0x35, 0x03}},
    {{0x07, 0x00, 0x00, 0x71, 0x01, 0x00, 0x72, 0x00, 0x20}, {0x1F, 0x03}},
});

constexpr std::uint8_t CameraControlStart = 0x08;
constexpr std::uint8_t CameraBasicMode = 0x01;

// The speaker for 4 bit Yamaha ADPCM (WiiBrew, Wiimote, Speaker): 0x07D0 is the rate value of the HOME menu too.
constexpr std::uint8_t SpeakerSwitchOn = 0x01;
constexpr std::uint8_t SpeakerConfigurationStart = 0x08;
constexpr std::uint8_t SpeakerPlayOn = 0x01;
constexpr std::uint8_t SpeakerFormatAdpcm = 0x00;
constexpr std::uint8_t SpeakerRateLowByte = 0xD0;
constexpr std::uint8_t SpeakerRateHighByte = 0x07;

// Likely: the volume of the example of WiiBrew. The menu scales its samples itself.
constexpr std::uint8_t SpeakerVolume = 0x40;

// Writing these two values starts an extension without encryption (WiiBrew, Extension Controllers, "The New Way").
constexpr std::uint8_t ExtensionStartFirstValue = 0x55;
constexpr std::uint8_t ExtensionStartSecondValue = 0x00;

// The identities of the extensions: the last four of the six bytes (WiiBrew, Extension Controllers).
constexpr std::array<std::uint8_t, 4> NunchukIdentity = {0xA4, 0x20, 0x00, 0x00};
constexpr std::array<std::uint8_t, 4> ClassicControllerIdentity = {0xA4, 0x20, 0x01, 0x01};
constexpr std::size_t IdentityKindByte = 2;

// Assumed: the zero and one gravity of a remote and a Nunchuk that has not sent its calibration, in units of 10 bits.
constexpr int UsualAccelerationZero = 512;
constexpr int UsualRemoteOneGravity = 616;
constexpr int UsualNunchukOneGravity = 716;

// The Nunchuk (WiiBrew, Nunchuck): stick x, stick y, acceleration x, y, z (high 8 bits), and a byte with Z, C and the low bits.
constexpr std::size_t NunchukStickXByte = 0;
constexpr std::size_t NunchukStickYByte = 1;
constexpr std::size_t NunchukAccelerationXByte = 2;
constexpr std::size_t NunchukAccelerationYByte = 3;
constexpr std::size_t NunchukAccelerationZByte = 4;
constexpr std::size_t NunchukButtonByte = 5;
constexpr std::uint8_t NunchukZReleased = 0x01;
constexpr std::uint8_t NunchukCReleased = 0x02;
constexpr int NunchukAccelerationXShift = 2;
constexpr int NunchukAccelerationYShift = 4;
constexpr int NunchukAccelerationZShift = 6;
constexpr int NunchukStickMiddle = 128;

// The pointer library makes a stick axis (|raw| - dead zone) / (full - dead zone), clamped to 0 to 1 with the sign kept:
// 15 and 71 for the Nunchuk, 60 and 308 for the sticks of the Classic Controller, 30 and 180 for its triggers.
constexpr float NunchukDeadZone = 15;
constexpr float NunchukFull = 71;
constexpr float ClassicStickDeadZone = 60;
constexpr float ClassicStickFull = 308;
constexpr float ClassicTriggerDeadZone = 30;
constexpr float ClassicTriggerFull = 180;

// Assumed: the library's raw values of the Classic Controller are its 6 bit left stick times 16, its 5 bit right stick
// times 32 (both from their middle) and its 5 bit triggers times 8.
constexpr int ClassicLeftStickMiddle = 32;
constexpr int ClassicLeftStickScale = 16;
constexpr int ClassicRightStickMiddle = 16;
constexpr int ClassicRightStickScale = 32;
constexpr int ClassicTriggerScale = 8;

// The Classic Controller (WiiBrew, Classic Controller): the sticks and triggers are spread over the first four bytes, the buttons
// are the last two, a button is 0 while it is pressed.
constexpr int SixBits = 0x3F;
constexpr int FiveBits = 0x1F;
constexpr std::size_t ClassicFirstButtonByte = 4;
constexpr std::size_t ClassicSecondButtonByte = 5;
constexpr ButtonMask AllClassicButtons = 0xFEFF;

std::uint8_t HighByte(std::uint32_t address)
{
    return static_cast<std::uint8_t>(address >> (2 * ByteShift));
}

std::uint8_t MiddleByte(std::uint32_t address)
{
    return static_cast<std::uint8_t>(address >> ByteShift);
}

std::uint8_t LowByte(std::uint32_t address)
{
    return static_cast<std::uint8_t>(address);
}

int TwoBitsAt(int byte, int shift)
{
    return (byte >> shift) & TwoBits;
}

int BitAt(int byte, int bit)
{
    return (byte >> bit) & 1;
}

int TenBitValue(std::uint8_t highEightBits, int lowTwoBits)
{
    return (highEightBits << 2) | lowTwoBits;
}

// One group of the basic form holds two dots in five bytes: x1, y1, the high bits of both, x2, y2.
void ReadDotGroup(std::span<const std::uint8_t> group, std::vector<InfraredDot>& dots)
{
    int highBits = group[2];
    int firstX = group[0] | (TwoBitsAt(highBits, 4) << ByteShift);
    int firstY = group[1] | (TwoBitsAt(highBits, 6) << ByteShift);
    int secondX = group[3] | (TwoBitsAt(highBits, 0) << ByteShift);
    int secondY = group[4] | (TwoBitsAt(highBits, 2) << ByteShift);

    if (firstY < CameraPictureHeight)
        dots.push_back({static_cast<float>(firstX), static_cast<float>(firstY)});
    if (secondY < CameraPictureHeight)
        dots.push_back({static_cast<float>(secondX), static_cast<float>(secondY)});
}

float GravitiesOfAxis(int raw, int zero, int oneGravity)
{
    int gravitySize = oneGravity - zero;
    if (gravitySize == 0)
        return 0;

    return static_cast<float>(raw - zero) / static_cast<float>(gravitySize);
}

// The rule of the pointer library for one axis of a stick or a trigger.
float ScaleAxis(float raw, float deadZone, float full)
{
    float size = (std::abs(raw) - deadZone) / (full - deadZone);
    size = std::clamp(size, 0.0f, 1.0f);
    return raw < 0 ? -size : size;
}

// The library keeps the stick inside the circle of radius 1.
Formats::Vector2 LimitToCircle(Formats::Vector2 stick)
{
    float length = std::sqrt(stick.x * stick.x + stick.y * stick.y);
    if (length <= 1)
        return stick;

    return {stick.x / length, stick.y / length};
}

bool EndsWith(std::span<const std::uint8_t> identity, std::span<const std::uint8_t> ending)
{
    return std::equal(ending.begin(), ending.end(), identity.begin() + IdentityKindByte);
}

}

ReportBytes MakeRumbleReport()
{
    return {WiiRemoteReportId::Rumble, 0};
}

ReportBytes MakePlayerLedsReport(const PlayerLeds& leds)
{
    std::uint8_t lights = 0;
    for (int led = 0; led < PlayerLedCount; ++led)
    {
        if (leds[led])
            lights |= static_cast<std::uint8_t>(FirstLedBit << led);
    }

    return {WiiRemoteReportId::PlayerLeds, lights};
}

ReportBytes MakeReportingModeReport(std::uint8_t dataReportId, bool isContinuous)
{
    std::uint8_t flags = isContinuous ? ContinuousBit : 0;
    return {WiiRemoteReportId::ReportingMode, flags, dataReportId};
}

ReportBytes MakeSwitchReport(std::uint8_t reportId, bool isOn)
{
    std::uint8_t flags = isOn ? SwitchOnBit : 0;
    return {reportId, flags};
}

ReportBytes MakeStatusRequest()
{
    return {WiiRemoteReportId::StatusRequest, 0};
}

ReportBytes MakeRegisterWrite(std::uint32_t address, std::span<const std::uint8_t> bytes)
{
    std::size_t byteCount = std::min(bytes.size(), LargestWrite);
    ReportBytes report = {WiiRemoteReportId::WriteMemory, RegisterSpaceBit, HighByte(address), MiddleByte(address), LowByte(address),
        static_cast<std::uint8_t>(byteCount)};
    report.insert(report.end(), bytes.begin(), bytes.begin() + byteCount);
    report.resize(report.size() + LargestWrite - byteCount, 0);
    return report;
}

ReportBytes MakeRegisterWrite(std::uint32_t address, std::uint8_t value)
{
    std::array<std::uint8_t, 1> bytes = {value};
    return MakeRegisterWrite(address, bytes);
}

ReportBytes MakeMemoryRead(std::uint32_t address, std::uint16_t byteCount)
{
    std::uint8_t space = address >= FirstRegisterAddress ? RegisterSpaceBit : 0;
    return {WiiRemoteReportId::ReadMemory, space, HighByte(address), MiddleByte(address), LowByte(address), MiddleByte(byteCount),
        LowByte(byteCount)};
}

ReportBytes MakeSpeakerDataReport(std::span<const std::uint8_t> bytes)
{
    std::size_t byteCount = std::min(bytes.size(), LargestSpeakerPacket);
    ReportBytes report = {WiiRemoteReportId::SpeakerData, static_cast<std::uint8_t>(byteCount << SpeakerSizeShift)};
    report.insert(report.end(), bytes.begin(), bytes.begin() + byteCount);
    report.resize(report.size() + LargestSpeakerPacket - byteCount, 0);
    return report;
}

void SetRumbleBit(ReportBytes& report, bool isRumbling)
{
    if (report.size() < 2)
        return;

    if (isRumbling)
        report[1] |= RumbleBit;
    else
        report[1] &= static_cast<std::uint8_t>(~RumbleBit);
}

std::vector<ReportBytes> MakeCameraStart(int sensitivityLevel)
{
    int level = std::clamp(sensitivityLevel, LowestCameraSensitivity, HighestCameraSensitivity);
    const SensitivityBlocks& blocks = SensitivityLevels[level - LowestCameraSensitivity];

    return {
        MakeSwitchReport(WiiRemoteReportId::InfraredCamera, true),
        MakeSwitchReport(WiiRemoteReportId::InfraredCameraSecondSwitch, true),
        MakeRegisterWrite(WiiRemoteAddress::CameraControl, CameraControlStart),
        MakeRegisterWrite(WiiRemoteAddress::CameraSensitivityFirst, blocks.first),
        MakeRegisterWrite(WiiRemoteAddress::CameraSensitivitySecond, blocks.second),
        MakeRegisterWrite(WiiRemoteAddress::CameraMode, CameraBasicMode),
        MakeRegisterWrite(WiiRemoteAddress::CameraControl, CameraControlStart),
    };
}

std::vector<ReportBytes> MakeSpeakerStart()
{
    // An unused byte, the format, the rate (low byte first), the volume and two unused bytes.
    std::array<std::uint8_t, 7> configuration = {
        0x00, SpeakerFormatAdpcm, SpeakerRateLowByte, SpeakerRateHighByte, SpeakerVolume, 0x00, 0x00};

    return {
        MakeSwitchReport(WiiRemoteReportId::Speaker, true),
        MakeSwitchReport(WiiRemoteReportId::SpeakerMute, true),
        MakeRegisterWrite(WiiRemoteAddress::SpeakerSwitch, SpeakerSwitchOn),
        MakeRegisterWrite(WiiRemoteAddress::SpeakerConfiguration, SpeakerConfigurationStart),
        MakeRegisterWrite(WiiRemoteAddress::SpeakerConfiguration, configuration),
        MakeRegisterWrite(WiiRemoteAddress::SpeakerPlay, SpeakerPlayOn),
        MakeSwitchReport(WiiRemoteReportId::SpeakerMute, false),
    };
}

std::vector<ReportBytes> MakeExtensionStart()
{
    return {
        MakeRegisterWrite(WiiRemoteAddress::ExtensionStartFirst, ExtensionStartFirstValue),
        MakeRegisterWrite(WiiRemoteAddress::ExtensionStartSecond, ExtensionStartSecondValue),
        MakeMemoryRead(WiiRemoteAddress::ExtensionIdentity, ExtensionIdentitySize),
    };
}

std::optional<ButtonMask> ReadButtons(std::span<const std::uint8_t> report)
{
    if (report.size() <= SecondButtonByte)
        return std::nullopt;

    bool hasButtons = report[0] >= WiiRemoteReportId::Status && report[0] <= WiiRemoteReportId::ButtonsMotionDotsExtension;
    if (!hasButtons)
        return std::nullopt;

    ButtonMask buttons = report[FirstButtonByte] | (report[SecondButtonByte] << ByteShift);
    return buttons & AllRemoteButtons;
}

std::optional<WiiRemoteStatus> ReadStatusReport(std::span<const std::uint8_t> report)
{
    if (report.size() < StatusReportSize || report[0] != WiiRemoteReportId::Status)
        return std::nullopt;

    std::uint8_t flags = report[StatusFlagsByte];

    WiiRemoteStatus status;
    status.buttons = *ReadButtons(report);
    status.hasExtension = (flags & StatusHasExtension) != 0;
    status.isSpeakerOn = (flags & StatusSpeakerOn) != 0;
    status.isCameraOn = (flags & StatusCameraOn) != 0;
    status.batteryLevel = BatteryLevelOfByte(report[StatusBatteryByte]);
    return status;
}

std::optional<MemoryData> ReadMemoryDataReport(std::span<const std::uint8_t> report)
{
    if (report.size() < MemoryDataReportSize || report[0] != WiiRemoteReportId::MemoryData)
        return std::nullopt;

    int sizeAndError = report[MemorySizeAndErrorByte];
    int byteCount = (sizeAndError >> NibbleShift) + 1;

    MemoryData data;
    data.addressLowBits = static_cast<std::uint16_t>((report[MemoryAddressHighByte] << ByteShift) | report[MemoryAddressLowByte]);
    data.hasFailed = (sizeAndError & LowNibble) != 0;
    data.bytes.assign(report.begin() + MemoryFirstDataByte, report.begin() + MemoryFirstDataByte + byteCount);
    return data;
}

std::optional<WiiRemoteData> ReadDataReport(std::span<const std::uint8_t> report)
{
    if (report.size() < DataReportSize || report[0] != WiiRemoteReportId::ButtonsMotionDotsExtension)
        return std::nullopt;

    int firstButtonByte = report[FirstButtonByte];
    int secondButtonByte = report[SecondButtonByte];

    WiiRemoteData data;
    data.buttons = *ReadButtons(report);
    // The low bits of the acceleration ride in the button bytes: bits 5 and 6 of the first byte are the low two bits of x, bit 5 of
    // the second byte is bit 1 of y and bit 6 is bit 1 of z. Bit 0 of y and z is not sent.
    data.acceleration.x = TenBitValue(report[AccelerationXByte], TwoBitsAt(firstButtonByte, 5));
    data.acceleration.y = TenBitValue(report[AccelerationYByte], BitAt(secondButtonByte, 5) << 1);
    data.acceleration.z = TenBitValue(report[AccelerationZByte], BitAt(secondButtonByte, 6) << 1);

    ReadDotGroup(report.subspan(FirstDotGroupByte), data.dots);
    ReadDotGroup(report.subspan(SecondDotGroupByte), data.dots);

    std::copy_n(report.begin() + ExtensionFirstByte, ExtensionIdentitySize, data.extension.begin());
    return data;
}

int BatteryLevelOfByte(std::uint8_t batteryByte)
{
    for (std::size_t level = 0; level < BatteryLevelLimits.size(); ++level)
    {
        if (batteryByte < BatteryLevelLimits[level])
            return static_cast<int>(level);
    }

    return MaximumBatteryLevel;
}

// Each half is three bytes with the high 8 bits of x, y and z, and a byte with their low two bits: x at bits 4 and 5, y at 2 and 3, z at 0 and 1.
AccelerationCalibration ReadAccelerationCalibration(std::span<const std::uint8_t, AccelerationCalibrationSize> bytes)
{
    AccelerationCalibration calibration;
    calibration.zero.x = TenBitValue(bytes[0], TwoBitsAt(bytes[3], 4));
    calibration.zero.y = TenBitValue(bytes[1], TwoBitsAt(bytes[3], 2));
    calibration.zero.z = TenBitValue(bytes[2], TwoBitsAt(bytes[3], 0));
    calibration.oneGravity.x = TenBitValue(bytes[4], TwoBitsAt(bytes[7], 4));
    calibration.oneGravity.y = TenBitValue(bytes[5], TwoBitsAt(bytes[7], 2));
    calibration.oneGravity.z = TenBitValue(bytes[6], TwoBitsAt(bytes[7], 0));
    return calibration;
}

AccelerationCalibration UsualAccelerationCalibration()
{
    RawAcceleration zero = {UsualAccelerationZero, UsualAccelerationZero, UsualAccelerationZero};
    RawAcceleration oneGravity = {UsualRemoteOneGravity, UsualRemoteOneGravity, UsualRemoteOneGravity};
    return {zero, oneGravity};
}

Formats::Vector3 AccelerationInGravities(RawAcceleration acceleration, const AccelerationCalibration& calibration)
{
    return {
        GravitiesOfAxis(acceleration.x, calibration.zero.x, calibration.oneGravity.x),
        GravitiesOfAxis(acceleration.y, calibration.zero.y, calibration.oneGravity.y),
        GravitiesOfAxis(acceleration.z, calibration.zero.z, calibration.oneGravity.z),
    };
}

ExtensionKind IdentifyExtension(std::span<const std::uint8_t> identity)
{
    if (identity.size() < ExtensionIdentitySize)
        return ExtensionKind::Unsupported;

    if (EndsWith(identity, NunchukIdentity))
        return ExtensionKind::Nunchuk;

    if (EndsWith(identity, ClassicControllerIdentity))
        return ExtensionKind::ClassicController;

    return ExtensionKind::Unsupported;
}

NunchukReading ReadNunchuk(std::span<const std::uint8_t, ExtensionIdentitySize> bytes)
{
    int buttonByte = bytes[NunchukButtonByte];

    NunchukReading reading;
    if ((buttonByte & NunchukZReleased) == 0)
        reading.buttons |= RemoteButtons::NunchukZ;
    if ((buttonByte & NunchukCReleased) == 0)
        reading.buttons |= RemoteButtons::NunchukC;

    float stickX = ScaleAxis(static_cast<float>(bytes[NunchukStickXByte] - NunchukStickMiddle), NunchukDeadZone, NunchukFull);
    float stickY = ScaleAxis(static_cast<float>(bytes[NunchukStickYByte] - NunchukStickMiddle), NunchukDeadZone, NunchukFull);
    reading.state.stick = LimitToCircle({stickX, stickY});

    RawAcceleration acceleration;
    acceleration.x = TenBitValue(bytes[NunchukAccelerationXByte], TwoBitsAt(buttonByte, NunchukAccelerationXShift));
    acceleration.y = TenBitValue(bytes[NunchukAccelerationYByte], TwoBitsAt(buttonByte, NunchukAccelerationYShift));
    acceleration.z = TenBitValue(bytes[NunchukAccelerationZByte], TwoBitsAt(buttonByte, NunchukAccelerationZShift));

    RawAcceleration zero = {UsualAccelerationZero, UsualAccelerationZero, UsualAccelerationZero};
    RawAcceleration oneGravity = {UsualNunchukOneGravity, UsualNunchukOneGravity, UsualNunchukOneGravity};
    reading.state.acceleration = AccelerationInGravities(acceleration, {zero, oneGravity});
    return reading;
}

// Byte 0: right stick x bits 4 and 3, left stick x. Byte 1: right stick x bits 2 and 1, left stick y.
// Byte 2: right stick x bit 0, left trigger bits 4 and 3, right stick y. Byte 3: left trigger bits 2 to 0, right trigger.
ClassicControllerState ReadClassicController(std::span<const std::uint8_t, ExtensionIdentitySize> bytes)
{
    int leftX = bytes[0] & SixBits;
    int leftY = bytes[1] & SixBits;
    int rightXHighBits = bytes[0] >> 6;
    int rightXMiddleBits = bytes[1] >> 6;
    int rightXLowBit = bytes[2] >> 7;
    int rightX = (rightXHighBits << 3) | (rightXMiddleBits << 1) | rightXLowBit;
    int rightY = bytes[2] & FiveBits;
    int leftTrigger = (((bytes[2] >> 5) & TwoBits) << 3) | (bytes[3] >> 5);
    int rightTrigger = bytes[3] & FiveBits;

    float leftRawX = static_cast<float>((leftX - ClassicLeftStickMiddle) * ClassicLeftStickScale);
    float leftRawY = static_cast<float>((leftY - ClassicLeftStickMiddle) * ClassicLeftStickScale);
    float rightRawX = static_cast<float>((rightX - ClassicRightStickMiddle) * ClassicRightStickScale);
    float rightRawY = static_cast<float>((rightY - ClassicRightStickMiddle) * ClassicRightStickScale);

    ClassicControllerState state;
    state.leftStick = LimitToCircle({ScaleAxis(leftRawX, ClassicStickDeadZone, ClassicStickFull),
        ScaleAxis(leftRawY, ClassicStickDeadZone, ClassicStickFull)});
    state.rightStick = LimitToCircle({ScaleAxis(rightRawX, ClassicStickDeadZone, ClassicStickFull),
        ScaleAxis(rightRawY, ClassicStickDeadZone, ClassicStickFull)});
    state.leftTrigger = ScaleAxis(static_cast<float>(leftTrigger * ClassicTriggerScale), ClassicTriggerDeadZone, ClassicTriggerFull);
    state.rightTrigger = ScaleAxis(static_cast<float>(rightTrigger * ClassicTriggerScale), ClassicTriggerDeadZone, ClassicTriggerFull);

    ButtonMask pressedBitsAreZero = (bytes[ClassicFirstButtonByte] << ByteShift) | bytes[ClassicSecondButtonByte];
    state.buttonsHeld = ~pressedBitsAreZero & AllClassicButtons;
    return state;
}

}
