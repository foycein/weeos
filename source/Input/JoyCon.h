#pragma once

#include "Input/JoyConReports.h"
#include "Input/RemoteOutputs.h"

#include "Platform/HidConnection.h"

#include <deque>
#include <memory>

namespace Input
{

// One Joy-Con over Bluetooth. When it is opened it is switched to its full report with the motion and asked for its calibration;
// a Joy-Con that sends nothing for a while is not connected and is set up again every two seconds.
class JoyCon
{
public:
    JoyCon(std::unique_ptr<Platform::HidConnection> connection, JoyConSide side);

    // Takes every report that has arrived and sends what is due, once in every step.
    void Poll(double nowInSeconds);

    // False once Windows has taken the device away.
    bool IsPresent() const;

    // True while it sends its reports.
    bool IsConnected() const;

    JoyConSide Side() const;

    // The buttons of its own side and the shared ones (JoyConButtons), its own stick, and its battery level 0 to 4.
    std::uint32_t Buttons() const;
    Formats::Vector2 Stick() const;
    int BatteryLevel() const;

    // Whether a report has come since the last call.
    bool TakeNewData();

    // How far it has turned since the last call, in degrees around its x, y and z axes.
    Formats::Vector3 TakeTurn();

    void SetRumble(bool shouldRumble);
    void SetPlayerLeds(const PlayerLeds& leds);

private:
    void HandleReport(const ReportBytes& report);
    void HandleReply(const JoyConSubcommandReply& reply);
    void AddTurn(const std::array<int, 3>& sample);
    void SetUp();
    void Send(ReportBytes report);
    std::uint8_t NextPacketNumber();

    std::unique_ptr<Platform::HidConnection> connection;
    JoyConSide side;
    double now = 0;
    double lastReportTime = 0;
    double lastSetUpTime = 0;
    bool hasReport = false;

    std::deque<ReportBytes> waitingReports;
    std::uint8_t packetNumber = 0;

    JoyConStickCalibration stickCalibration = UsualStickCalibration();
    JoyConMotionCalibration motionCalibration = UsualMotionCalibration();

    std::uint32_t buttons = 0;
    RawStick rawStick;
    int batteryLevel = MaximumBatteryLevel;
    bool hasNewData = false;
    Formats::Vector3 turn = {0, 0, 0};

    bool isRumbling = false;
    PlayerLeds playerLeds = {};
};

}
