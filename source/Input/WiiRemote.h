#pragma once

#include "Input/InfraredPointer.h"
#include "Input/RemoteSource.h"
#include "Input/SpeakerAdpcm.h"
#include "Input/WiiRemoteReports.h"

#include "Platform/HidConnection.h"

#include <deque>
#include <memory>

namespace Input
{

// The camera sensitivity of a console without the setting BT.SENS is 2 (OpenPoints/Input.md, INP-02).
constexpr int UsualCameraSensitivity = 2;

// A real Wii Remote, over Bluetooth or through a Dolphin Bar in its Wii Remote mode: Windows shows both as a device of the class
// "human interface device", and the remote speaks the protocol of WiiBrew (Wiimote) through it.
//
// The remote counts as connected from its first report on. When it sends nothing for a while it is taken to be off (a Dolphin Bar
// keeps its four devices also without remotes), and it is asked for its status every second until it answers.
// At a connection it gets its data report (buttons, motion, camera, extension, about 100 in a second), its lights, its calibration
// and its camera; a Nunchuk or Classic Controller is started and named when the remote says one is plugged in.
// After Disconnect, and after five minutes without anything happening (INP-07), it sleeps: its lights go off, it is not connected,
// and a press of one of its buttons connects it again, as a remote that was switched off does.
class WiiRemote final : public RemoteSource
{
public:
    explicit WiiRemote(std::unique_ptr<Platform::HidConnection> connection);

    // Takes every report that has arrived and sends what is due. The program calls this in every step, with the seconds of a clock
    // that never goes backward.
    void Poll(double nowInSeconds);

    // False once Windows has taken the device away. It never comes back.
    bool IsPresent() const;

    // The sensitivity level of the camera (1 to 5), the setting BT.SENS of the console. A change restarts the camera.
    void SetCameraSensitivity(int level);

    ExtensionKind Extension() const;

    DeviceKind Probe() const override;

    // The new state after the reports since the last read. A button that went down and up between two reads is held for one read.
    std::optional<RemoteState> ReadNewState() override;

    void Disconnect() override;
    void SetSensorBarPosition(SensorBarPosition position) override;
    void SetRumble(bool shouldRumble) override;
    void SetPlayerLeds(const PlayerLeds& leds) override;

    // The sound is cut into packets of 40 samples and one packet goes out every 1/150 second (INP-01).
    void PlaySpeakerSound(const SpeakerSound& sound) override;

private:
    enum class LinkState
    {
        Silent,
        Connected,
        Asleep,
    };

    void HandleReport(const ReportBytes& report);
    void HandleStatus(const WiiRemoteStatus& status);
    void HandleMemoryData(const MemoryData& data);
    void HandleData(const WiiRemoteData& data);
    void NoticeButtons(ButtonMask buttons);
    void NoticeClassicButtons(ButtonMask buttons);
    void NoticeMotion(Formats::Vector3 acceleration);

    void Connect();
    void FallAsleep();
    void LoseConnection();
    void CheckTimers();

    void Send(ReportBytes report);
    void SendNextWaitingReport();
    void SendDueSpeakerPackets();
    void Wait(const std::vector<ReportBytes>& reports);

    RemoteState MakeState();
    float RollFromAcceleration() const;

    std::unique_ptr<Platform::HidConnection> connection;
    LinkState linkState = LinkState::Silent;
    double now = 0;
    double lastReportTime = 0;
    double lastProbeTime = 0;
    double lastStatusTime = 0;
    double lastActivityTime = 0;
    bool hasProbed = false;

    // Reports that set the remote up go out one in every step, so that the remote has time for each of them.
    std::deque<ReportBytes> waitingReports;

    ExtensionKind extension = ExtensionKind::None;
    AccelerationCalibration calibration = UsualAccelerationCalibration();
    InfraredPointer infraredPointer;
    int cameraSensitivity = UsualCameraSensitivity;

    bool isRumbling = false;
    PlayerLeds playerLeds = {};

    bool hasNewData = false;
    ButtonMask latestButtons = 0;
    ButtonMask buttonsPressedSinceLastRead = 0;
    ButtonMask buttonsHeldAtLastRead = 0;
    ButtonMask buttonsWhileAsleep = 0;
    ButtonMask latestClassicButtons = 0;
    ButtonMask classicPressedSinceLastRead = 0;
    ButtonMask classicHeldAtLastRead = 0;
    Formats::Vector3 latestAcceleration = {0, 0, 1};
    Formats::Vector3 accelerationAtLastActivity = {0, 0, 1};
    NunchukState latestNunchuk;
    ClassicControllerState latestClassic;
    int batteryLevel = MaximumBatteryLevel;

    bool isSpeakerStarted = false;
    bool isSpeakerStreaming = false;
    double speakerStreamStart = 0;
    int speakerPacketsSent = 0;
    SpeakerAdpcmEncoder speakerEncoder;
    std::deque<std::vector<std::uint8_t>> speakerPackets;
};

}
