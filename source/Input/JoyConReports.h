#pragma once

#include "Input/WiiRemoteReports.h"

#include "Formats/Vectors.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Input
{

// The reports of a Joy-Con over Bluetooth, from the public notes on the Joy-Con's protocol (dekuNukem,
// Nintendo_Switch_Reverse_Engineering: bluetooth_hid_notes, bluetooth_hid_subcommands_notes, spi_flash_notes, imu_sensor_notes).

enum class JoyConSide
{
    Left,
    Right,
};

namespace JoyConReportId
{

// To the Joy-Con: a subcommand with the rumble, or the rumble alone.
constexpr std::uint8_t Subcommand = 0x01;
constexpr std::uint8_t RumbleOnly = 0x10;

// From the Joy-Con: the answer to a subcommand, and the full report with the motion, 60 times in a second.
constexpr std::uint8_t SubcommandReply = 0x21;
constexpr std::uint8_t FullReport = 0x30;

}

namespace JoyConSubcommand
{

constexpr std::uint8_t SetReportMode = 0x03;
constexpr std::uint8_t ReadFlash = 0x10;
constexpr std::uint8_t SetPlayerLights = 0x30;
constexpr std::uint8_t EnableMotion = 0x40;
constexpr std::uint8_t EnableVibration = 0x48;

}

// Where the factory calibration lies in the flash memory of a Joy-Con, and how long it is.
namespace JoyConFlash
{

constexpr std::uint32_t MotionCalibration = 0x6020;
constexpr std::uint32_t LeftStickCalibration = 0x603D;
constexpr std::uint32_t RightStickCalibration = 0x6046;
constexpr std::uint8_t MotionCalibrationSize = 24;
constexpr std::uint8_t StickCalibrationSize = 9;

}

// The buttons of both Joy-Cons as one number: the byte of the right Joy-Con, the shared byte above it, the byte of the left one on top.
namespace JoyConButtons
{

constexpr std::uint32_t Y = 0x000001;
constexpr std::uint32_t X = 0x000002;
constexpr std::uint32_t B = 0x000004;
constexpr std::uint32_t A = 0x000008;
constexpr std::uint32_t RightSR = 0x000010;
constexpr std::uint32_t RightSL = 0x000020;
constexpr std::uint32_t R = 0x000040;
constexpr std::uint32_t ZR = 0x000080;
constexpr std::uint32_t Minus = 0x000100;
constexpr std::uint32_t Plus = 0x000200;
constexpr std::uint32_t RightStickClick = 0x000400;
constexpr std::uint32_t LeftStickClick = 0x000800;
constexpr std::uint32_t Home = 0x001000;
constexpr std::uint32_t Capture = 0x002000;
constexpr std::uint32_t Down = 0x010000;
constexpr std::uint32_t Up = 0x020000;
constexpr std::uint32_t Right = 0x040000;
constexpr std::uint32_t Left = 0x080000;
constexpr std::uint32_t LeftSR = 0x100000;
constexpr std::uint32_t LeftSL = 0x200000;
constexpr std::uint32_t L = 0x400000;
constexpr std::uint32_t ZL = 0x800000;

}

// A stick reports 12 bits on each axis. The calibration gives its middle and how far it goes up and down from there.
struct RawStick
{
    int x = 0;
    int y = 0;
};

struct JoyConStickCalibration
{
    RawStick middle;
    RawStick rangeAbove;
    RawStick rangeBelow;
};

// The gyroscope: its value at rest and the value its formula takes as the reference for 936 degrees a second.
struct JoyConMotionCalibration
{
    std::array<int, 3> gyroscopeRest = {};
    std::array<int, 3> gyroscopeReference = {};
};

// Three turn rates, in degrees per second, around the x, y and z axes of the Joy-Con.
using TurnRates = Formats::Vector3;

struct JoyConInput
{
    std::uint32_t buttons = 0;
    int batteryLevel = MaximumBatteryLevel;
    RawStick leftStick;
    RawStick rightStick;

    // The full report has three samples of the gyroscope, 5 ms apart. The answer to a subcommand has none.
    std::vector<std::array<int, 3>> gyroscopeSamples;
};

struct JoyConSubcommandReply
{
    std::uint8_t subcommand = 0;
    bool isAcknowledged = false;
    std::vector<std::uint8_t> data;
};

// The seconds between two samples of the gyroscope.
constexpr double JoyConSampleSeconds = 0.005;

// ---- Reports to the Joy-Con. The packet number goes from 0 to 15 and on again.

ReportBytes MakeJoyConSubcommand(std::uint8_t packetNumber, bool isRumbling, std::uint8_t subcommand, std::span<const std::uint8_t> arguments);
ReportBytes MakeJoyConRumble(std::uint8_t packetNumber, bool isRumbling);
ReportBytes MakeJoyConFlashRead(std::uint8_t packetNumber, std::uint32_t address, std::uint8_t size);

// The four lights at the rail light up from the top, one for each of the four players.
std::uint8_t JoyConPlayerLightsArgument(const PlayerLeds& leds);

// ---- Reports from the Joy-Con

// The buttons, battery and sticks of a full report or the answer to a subcommand, and the gyroscope of a full report.
std::optional<JoyConInput> ReadJoyConInput(std::span<const std::uint8_t> report);
std::optional<JoyConSubcommandReply> ReadJoyConSubcommandReply(std::span<const std::uint8_t> report);

// The answer to a read of the flash: the address, the size, then the bytes. Nothing when the answer is for another address.
std::optional<std::vector<std::uint8_t>> FlashBytesOfReply(const JoyConSubcommandReply& reply, std::uint32_t address);

JoyConStickCalibration ReadStickCalibration(std::span<const std::uint8_t> bytes, JoyConSide side);
JoyConMotionCalibration ReadMotionCalibration(std::span<const std::uint8_t> bytes);

// What a Joy-Con is taken to have until it has sent its own calibration.
JoyConStickCalibration UsualStickCalibration();
JoyConMotionCalibration UsualMotionCalibration();

// From -1 to 1, y upward, with a dead zone in the middle.
Formats::Vector2 StickPosition(RawStick stick, const JoyConStickCalibration& calibration);

TurnRates TurnRatesOf(const std::array<int, 3>& sample, const JoyConMotionCalibration& calibration);

}
