#include "Input/WiiRemote.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Input
{

namespace
{

// Assumed: a remote that sends nothing for 2 seconds while it should send 100
// reports in every second is off, and a silent one is asked every second whether it is there.
constexpr double SilenceBeforeLost = 2.0;
constexpr double SecondsBetweenProbes = 1.0;

// The battery is asked for once a minute.
constexpr double SecondsBetweenStatusRequests = 60.0;

// The library switches a remote off after 5 minutes without a change.
constexpr double IdleSecondsBeforeSleep = 5.0 * 60.0;

// Assumed: a remote that moves less than this lies still; noise of the sensor stays below it.
constexpr float StillMotionInGravities = 0.05f;

// The HOME menu streams 40 samples every 1/150 second, which is 20 bytes of ADPCM.
constexpr double SpeakerPacketsPerSecond = 150.0;
constexpr std::size_t SamplesPerSpeakerPacket = 40;

// The addresses of the two reads, as the answer gives their low 16 bits.
constexpr std::uint16_t CalibrationAddressLowBits = WiiRemoteAddress::AccelerationCalibration & 0xFFFF;
constexpr std::uint16_t ExtensionIdentityLowBits = WiiRemoteAddress::ExtensionIdentity & 0xFFFF;

float LengthOfDifference(Formats::Vector3 first, Formats::Vector3 second)
{
    float x = first.x - second.x;
    float y = first.y - second.y;
    float z = first.z - second.z;
    return std::sqrt(x * x + y * y + z * z);
}

// shortcut: a sound of another rate takes the nearest sample; the menu's sounds are all made for the speaker's rate.
std::vector<std::int16_t> SamplesAtSpeakerRate(const SpeakerSound& sound)
{
    if (sound.sampleRate == RemoteSpeakerSampleRate || sound.sampleRate == 0)
        return sound.samples;

    std::size_t sampleCount = sound.samples.size() * RemoteSpeakerSampleRate / sound.sampleRate;
    std::vector<std::int16_t> samples(sampleCount);
    for (std::size_t index = 0; index < sampleCount; ++index)
        samples[index] = sound.samples[index * sound.sampleRate / RemoteSpeakerSampleRate];

    return samples;
}

}

WiiRemote::WiiRemote(std::unique_ptr<Platform::HidConnection> hidConnection)
    : connection(std::move(hidConnection))
{
}

void WiiRemote::Poll(double nowInSeconds)
{
    now = nowInSeconds;

    while (std::optional<ReportBytes> report = connection->TakeReport())
        HandleReport(*report);

    CheckTimers();
    SendNextWaitingReport();
    SendDueSpeakerPackets();
}

bool WiiRemote::IsPresent() const
{
    return connection->IsOpen();
}

void WiiRemote::SetCameraSensitivity(int level)
{
    if (level == cameraSensitivity)
        return;

    cameraSensitivity = level;
    if (linkState == LinkState::Connected)
        Wait(MakeCameraStart(cameraSensitivity));
}

ExtensionKind WiiRemote::Extension() const
{
    return extension;
}

DeviceKind WiiRemote::Probe() const
{
    if (linkState != LinkState::Connected)
        return DeviceKind::None;

    if (extension == ExtensionKind::Nunchuk)
        return DeviceKind::RemoteWithNunchuk;

    if (extension == ExtensionKind::ClassicController)
        return DeviceKind::RemoteWithClassicController;

    return DeviceKind::Remote;
}

std::optional<RemoteState> WiiRemote::ReadNewState()
{
    if (linkState != LinkState::Connected || !hasNewData)
        return std::nullopt;

    hasNewData = false;
    return MakeState();
}

void WiiRemote::Disconnect()
{
    if (linkState == LinkState::Connected)
        FallAsleep();
}

void WiiRemote::SetSensorBarPosition(SensorBarPosition position)
{
    infraredPointer.SetSensorBarPosition(position);
}

void WiiRemote::SetRumble(bool shouldRumble)
{
    if (shouldRumble == isRumbling)
        return;

    isRumbling = shouldRumble;
    if (linkState == LinkState::Connected)
        Send(MakeRumbleReport());
}

void WiiRemote::SetPlayerLeds(const PlayerLeds& leds)
{
    playerLeds = leds;
    if (linkState == LinkState::Connected)
        Send(MakePlayerLedsReport(playerLeds));
}

void WiiRemote::PlaySpeakerSound(const SpeakerSound& sound)
{
    if (linkState != LinkState::Connected)
        return;

    if (!isSpeakerStarted)
    {
        Wait(MakeSpeakerStart());
        isSpeakerStarted = true;
    }

    std::vector<std::int16_t> samples = SamplesAtSpeakerRate(sound);
    for (std::size_t first = 0; first < samples.size(); first += SamplesPerSpeakerPacket)
    {
        std::size_t count = std::min(SamplesPerSpeakerPacket, samples.size() - first);
        std::span<const std::int16_t> packetSamples(samples.data() + first, count);
        speakerPackets.push_back(speakerEncoder.Encode(packetSamples));
    }
}

void WiiRemote::HandleReport(const ReportBytes& report)
{
    lastReportTime = now;

    if (linkState == LinkState::Silent)
        Connect();

    if (linkState == LinkState::Asleep)
    {
        std::optional<ButtonMask> buttons = ReadButtons(report);
        if (!buttons)
            return;

        ButtonMask newlyPressed = *buttons & ~buttonsWhileAsleep;
        buttonsWhileAsleep = *buttons;
        if (newlyPressed == 0)
            return;

        Connect();
    }

    if (std::optional<WiiRemoteStatus> status = ReadStatusReport(report))
        HandleStatus(*status);
    else if (std::optional<MemoryData> memory = ReadMemoryDataReport(report))
        HandleMemoryData(*memory);
    else if (std::optional<WiiRemoteData> data = ReadDataReport(report))
        HandleData(*data);
    else if (std::optional<ButtonMask> buttons = ReadButtons(report))
        NoticeButtonsOnly(*buttons);
}

// The report of the buttons alone, which a remote sends before it is set up and while it sleeps. The Nunchuk's buttons stay.
void WiiRemote::NoticeButtonsOnly(ButtonMask buttons)
{
    hasNewData = true;
    ButtonMask nunchukButtons = latestButtons & (RemoteButtons::NunchukZ | RemoteButtons::NunchukC);
    NoticeButtons(buttons | nunchukButtons);
}

// A remote that sends its status without being asked stops its data reports until it is told the reporting mode again (WiiBrew).
void WiiRemote::HandleStatus(const WiiRemoteStatus& status)
{
    batteryLevel = status.batteryLevel;
    ButtonMask nunchukButtons = latestButtons & (RemoteButtons::NunchukZ | RemoteButtons::NunchukC);
    NoticeButtons(status.buttons | nunchukButtons);

    if (!status.hasExtension)
    {
        extension = ExtensionKind::None;
        latestClassicButtons = 0;
    }

    bool isExtensionNew = status.hasExtension && extension == ExtensionKind::None;
    if (isExtensionNew)
    {
        extension = ExtensionKind::Unsupported;
        Wait(MakeExtensionStart());
    }

    Wait({MakeReportingModeReport(WiiRemoteReportId::ButtonsMotionDotsExtension, true)});
}

void WiiRemote::HandleMemoryData(const MemoryData& data)
{
    if (data.addressLowBits == ExtensionIdentityLowBits)
    {
        extension = data.hasFailed ? ExtensionKind::Unsupported : IdentifyExtension(data.bytes);
        return;
    }

    bool isCalibration = data.addressLowBits == CalibrationAddressLowBits && !data.hasFailed;
    if (isCalibration && data.bytes.size() >= AccelerationCalibrationSize)
        calibration = ReadAccelerationCalibration(std::span<const std::uint8_t, AccelerationCalibrationSize>(data.bytes.data(), AccelerationCalibrationSize));
}

void WiiRemote::HandleData(const WiiRemoteData& data)
{
    hasNewData = true;
    infraredPointer.Update(data.dots);
    NoticeMotion(AccelerationInGravities(data.acceleration, calibration));

    ButtonMask buttons = data.buttons;
    if (extension == ExtensionKind::Nunchuk)
    {
        NunchukReading nunchuk = ReadNunchuk(data.extension);
        latestNunchuk = nunchuk.state;
        buttons |= nunchuk.buttons;
    }

    if (extension == ExtensionKind::ClassicController)
    {
        latestClassic = ReadClassicController(data.extension);
        NoticeClassicButtons(latestClassic.buttonsHeld);
    }

    NoticeButtons(buttons);
}

void WiiRemote::NoticeButtons(ButtonMask buttons)
{
    if (buttons != latestButtons)
        lastActivityTime = now;

    buttonsPressedSinceLastRead |= buttons & ~latestButtons;
    latestButtons = buttons;
}

void WiiRemote::NoticeClassicButtons(ButtonMask buttons)
{
    if (buttons != latestClassicButtons)
        lastActivityTime = now;

    classicPressedSinceLastRead |= buttons & ~latestClassicButtons;
    latestClassicButtons = buttons;
}

void WiiRemote::NoticeMotion(Formats::Vector3 acceleration)
{
    latestAcceleration = acceleration;
    if (LengthOfDifference(acceleration, accelerationAtLastActivity) < StillMotionInGravities)
        return;

    accelerationAtLastActivity = acceleration;
    lastActivityTime = now;
}

// The data report, the lights, the calibration, the camera, and the status, which tells about an extension.
void WiiRemote::Connect()
{
    linkState = LinkState::Connected;
    lastActivityTime = now;
    lastStatusTime = now;
    waitingReports.clear();

    Wait({MakeReportingModeReport(WiiRemoteReportId::ButtonsMotionDotsExtension, true), MakePlayerLedsReport(playerLeds),
        MakeMemoryRead(WiiRemoteAddress::AccelerationCalibration, AccelerationCalibrationSize)});
    Wait(MakeCameraStart(cameraSensitivity));
    Wait({MakeStatusRequest()});
}

// The remote reports only changes of its buttons while it sleeps, and the program waits for a button to go down. A button that was
// held when it fell asleep does not wake it.
void WiiRemote::FallAsleep()
{
    linkState = LinkState::Asleep;
    isRumbling = false;
    playerLeds = {};
    waitingReports.clear();
    speakerPackets.clear();
    isSpeakerStreaming = false;
    isSpeakerStarted = false;
    extension = ExtensionKind::None;
    buttonsWhileAsleep = latestButtons;

    Send(MakePlayerLedsReport(playerLeds));
    Send(MakeReportingModeReport(WiiRemoteReportId::ButtonsOnly, false));
}

void WiiRemote::LoseConnection()
{
    linkState = LinkState::Silent;
    waitingReports.clear();
    speakerPackets.clear();
    isSpeakerStreaming = false;
    isSpeakerStarted = false;
    extension = ExtensionKind::None;
    latestButtons = 0;
    latestClassicButtons = 0;
    hasNewData = false;
}

void WiiRemote::CheckTimers()
{
    if (linkState == LinkState::Connected && now - lastReportTime > SilenceBeforeLost)
        LoseConnection();

    if (linkState == LinkState::Silent && (!hasProbed || now - lastProbeTime >= SecondsBetweenProbes))
    {
        hasProbed = true;
        lastProbeTime = now;
        Send(MakeStatusRequest());
    }

    if (linkState != LinkState::Connected)
        return;

    if (now - lastActivityTime > IdleSecondsBeforeSleep)
    {
        FallAsleep();
        return;
    }

    if (now - lastStatusTime >= SecondsBetweenStatusRequests)
    {
        lastStatusTime = now;
        Wait({MakeStatusRequest()});
    }
}

void WiiRemote::Send(ReportBytes report)
{
    SetRumbleBit(report, isRumbling);
    connection->SendReport(report);
}

void WiiRemote::SendNextWaitingReport()
{
    if (waitingReports.empty())
        return;

    Send(waitingReports.front());
    waitingReports.pop_front();
}

// The packets wait until the speaker is set up, then go out at the pace the speaker plays them.
void WiiRemote::SendDueSpeakerPackets()
{
    if (speakerPackets.empty())
    {
        isSpeakerStreaming = false;
        return;
    }

    if (!waitingReports.empty())
        return;

    if (!isSpeakerStreaming)
    {
        isSpeakerStreaming = true;
        speakerStreamStart = now;
        speakerPacketsSent = 0;
    }

    int packetsDue = static_cast<int>((now - speakerStreamStart) * SpeakerPacketsPerSecond) + 1;
    while (speakerPacketsSent < packetsDue && !speakerPackets.empty())
    {
        Send(MakeSpeakerDataReport(speakerPackets.front()));
        speakerPackets.pop_front();
        ++speakerPacketsSent;
    }
}

void WiiRemote::Wait(const std::vector<ReportBytes>& reports)
{
    waitingReports.insert(waitingReports.end(), reports.begin(), reports.end());
}

RemoteState WiiRemote::MakeState()
{
    RemoteState state;

    ButtonMask held = latestButtons | buttonsPressedSinceLastRead;
    state.buttonsHeld = held;
    state.buttonsPressed = held & ~buttonsHeldAtLastRead;
    state.buttonsReleased = buttonsHeldAtLastRead & ~held;
    buttonsHeldAtLastRead = held;
    buttonsPressedSinceLastRead = 0;

    state.pointerIsValid = infraredPointer.SeesSensorBar();
    if (state.pointerIsValid)
    {
        state.pointer = infraredPointer.Pointer();
        state.rollDegrees = infraredPointer.RollDegrees();
        state.distanceInMeters = infraredPointer.DistanceInMeters();
    }
    else
    {
        state.rollDegrees = RollFromAcceleration();
    }

    state.batteryLevel = batteryLevel;
    state.nunchuk = latestNunchuk;

    ButtonMask classicHeld = latestClassicButtons | classicPressedSinceLastRead;
    state.classic = latestClassic;
    state.classic.buttonsHeld = classicHeld;
    state.classic.buttonsPressed = classicHeld & ~classicHeldAtLastRead;
    state.classic.buttonsReleased = classicHeldAtLastRead & ~classicHeld;
    classicHeldAtLastRead = classicHeld;
    classicPressedSinceLastRead = 0;
    return state;
}

// Likely: x of the remote points to its left and z out of its face, so a remote turned counterclockwise by an angle
// measures x = -sin(angle) and z = cos(angle) of the gravity.
float WiiRemote::RollFromAcceleration() const
{
    float radians = std::atan2(-latestAcceleration.x, latestAcceleration.z);
    return radians * 180.0f / std::numbers::pi_v<float>;
}

}
