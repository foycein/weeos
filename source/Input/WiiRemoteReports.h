#pragma once

#include "Input/RemoteButtons.h"
#include "Input/RemoteOutputs.h"
#include "Input/RemoteState.h"

#include "Formats/Vectors.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Input
{

// The bytes of one report of a Wii Remote. The first byte is the number of the report. The protocol is the one of WiiBrew (Wiimote).
using ReportBytes = std::vector<std::uint8_t>;

namespace WiiRemoteReportId
{

// The reports a program sends to the remote. The lowest bit of the byte after the number switches the motor on in every one of them.
constexpr std::uint8_t Rumble = 0x10;
constexpr std::uint8_t PlayerLeds = 0x11;
constexpr std::uint8_t ReportingMode = 0x12;
constexpr std::uint8_t InfraredCamera = 0x13;
constexpr std::uint8_t Speaker = 0x14;
constexpr std::uint8_t StatusRequest = 0x15;
constexpr std::uint8_t WriteMemory = 0x16;
constexpr std::uint8_t ReadMemory = 0x17;
constexpr std::uint8_t SpeakerData = 0x18;
constexpr std::uint8_t SpeakerMute = 0x19;
constexpr std::uint8_t InfraredCameraSecondSwitch = 0x1A;

// The reports the remote sends.
constexpr std::uint8_t Status = 0x20;
constexpr std::uint8_t MemoryData = 0x21;
constexpr std::uint8_t Acknowledge = 0x22;
constexpr std::uint8_t ButtonsOnly = 0x30;

// The data report WeeOS asks for: buttons, motion, the dots of the camera in the basic form, and six bytes of the extension.
constexpr std::uint8_t ButtonsMotionDotsExtension = 0x37;

}

// Where things are in the memory of the remote. The ones from 0xA00000 on are registers of the parts inside it.
namespace WiiRemoteAddress
{

constexpr std::uint32_t AccelerationCalibration = 0x000016;
constexpr std::uint32_t SpeakerSwitch = 0xA20009;
constexpr std::uint32_t SpeakerConfiguration = 0xA20001;
constexpr std::uint32_t SpeakerPlay = 0xA20008;
constexpr std::uint32_t ExtensionStartFirst = 0xA400F0;
constexpr std::uint32_t ExtensionStartSecond = 0xA400FB;
constexpr std::uint32_t ExtensionIdentity = 0xA400FA;
constexpr std::uint32_t CameraSensitivityFirst = 0xB00000;
constexpr std::uint32_t CameraSensitivitySecond = 0xB0001A;
constexpr std::uint32_t CameraControl = 0xB00030;
constexpr std::uint32_t CameraMode = 0xB00033;

}

// The calibration block of the acceleration is 8 bytes: the zero point and the point of one gravity.
constexpr int AccelerationCalibrationSize = 8;

// The identity of an extension is 6 bytes.
constexpr int ExtensionIdentitySize = 6;

// The camera has 5 sensitivity levels, the setting "Sensitivity" of the console (BT.SENS).
constexpr int LowestCameraSensitivity = 1;
constexpr int HighestCameraSensitivity = 5;

// A dot that the camera of the remote sees, in pixels of its picture of 1024 x 768. x grows to the right and y downward.
struct InfraredDot
{
    float x;
    float y;
};

// The acceleration as the remote measures it: 10 bits for each direction.
struct RawAcceleration
{
    int x = 0;
    int y = 0;
    int z = 0;
};

struct AccelerationCalibration
{
    RawAcceleration zero;
    RawAcceleration oneGravity;
};

struct WiiRemoteStatus
{
    ButtonMask buttons = 0;
    bool hasExtension = false;
    bool isSpeakerOn = false;
    bool isCameraOn = false;
    int batteryLevel = MaximumBatteryLevel;
};

// The answer to a read of the memory: at most 16 bytes.
struct MemoryData
{
    // The low 16 bits of the address of the first byte.
    std::uint16_t addressLowBits = 0;
    bool hasFailed = false;
    std::vector<std::uint8_t> bytes;
};

// One data report of the kind ButtonsMotionDotsExtension.
struct WiiRemoteData
{
    ButtonMask buttons = 0;
    RawAcceleration acceleration;
    std::vector<InfraredDot> dots;
    std::array<std::uint8_t, ExtensionIdentitySize> extension = {};
};

enum class ExtensionKind
{
    None,
    Nunchuk,
    ClassicController,

    // Any other extension: the remote works without it.
    Unsupported,
};

struct NunchukReading
{
    NunchukState state;

    // RemoteButtons::NunchukZ and NunchukC, the way the menu sees them in the buttons of the remote.
    ButtonMask buttons = 0;
};

// ---- Reports to the remote

ReportBytes MakeRumbleReport();
ReportBytes MakePlayerLedsReport(const PlayerLeds& leds);

// Continuous: the remote sends the report about 100 times in a second, also when nothing changes. Otherwise only on a change.
ReportBytes MakeReportingModeReport(std::uint8_t dataReportId, bool isContinuous);

// For the reports that switch something on or off: the camera, its second switch, the speaker and the speaker's mute.
ReportBytes MakeSwitchReport(std::uint8_t reportId, bool isOn);

ReportBytes MakeStatusRequest();

// Writes at most 16 bytes into the registers of the parts of the remote.
ReportBytes MakeRegisterWrite(std::uint32_t address, std::span<const std::uint8_t> bytes);
ReportBytes MakeRegisterWrite(std::uint32_t address, std::uint8_t value);

// Asks for bytes of the memory of the remote (the calibration) or of its registers (the extension).
ReportBytes MakeMemoryRead(std::uint32_t address, std::uint16_t byteCount);

// One packet of sound for the speaker, at most 20 bytes.
ReportBytes MakeSpeakerDataReport(std::span<const std::uint8_t> bytes);

// Puts the motor bit into a report that is about to be sent, so that a report does not stop the rumble.
void SetRumbleBit(ReportBytes& report, bool isRumbling);

// The reports that start the camera in the basic mode at a sensitivity level (1 to 5), in their order: its two switches and the
// register writes of WiiBrew (IR Camera, "Initialization"). A level outside 1 to 5 counts as the nearest one.
std::vector<ReportBytes> MakeCameraStart(int sensitivityLevel);

// The reports that start the speaker for 4 bit ADPCM, in their order: on and muted, the register writes, the mute off (WiiBrew, Wiimote, Speaker).
std::vector<ReportBytes> MakeSpeakerStart();

// The writes that start an extension without its encryption, and the read of its identity.
std::vector<ReportBytes> MakeExtensionStart();

// ---- Reports from the remote

// The buttons are in the two bytes after the number in every report from 0x20 to 0x37.
std::optional<ButtonMask> ReadButtons(std::span<const std::uint8_t> report);

std::optional<WiiRemoteStatus> ReadStatusReport(std::span<const std::uint8_t> report);
std::optional<MemoryData> ReadMemoryDataReport(std::span<const std::uint8_t> report);
std::optional<WiiRemoteData> ReadDataReport(std::span<const std::uint8_t> report);

// The level 0 to 4 of the byte of the battery in a status report.
int BatteryLevelOfByte(std::uint8_t batteryByte);

AccelerationCalibration ReadAccelerationCalibration(std::span<const std::uint8_t, AccelerationCalibrationSize> bytes);

// The calibration a remote is taken to have until it has sent its own.
AccelerationCalibration UsualAccelerationCalibration();

// In units of the gravity. A remote lying face up on a table gives (0, 0, 1).
Formats::Vector3 AccelerationInGravities(RawAcceleration acceleration, const AccelerationCalibration& calibration);

ExtensionKind IdentifyExtension(std::span<const std::uint8_t> identity);

// The six bytes of the extension, as the extension sends them without encryption.
NunchukReading ReadNunchuk(std::span<const std::uint8_t, ExtensionIdentitySize> bytes);

// Only the held buttons, the sticks and the triggers; the edges are worked out by whoever compares two states.
ClassicControllerState ReadClassicController(std::span<const std::uint8_t, ExtensionIdentitySize> bytes);

}
