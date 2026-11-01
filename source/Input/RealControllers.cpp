#include "Input/RealControllers.h"

#include <algorithm>
#include <format>

namespace Input
{

namespace
{

constexpr double SecondsBetweenSearches = 2.0;

bool IsWiiRemote(const Platform::HidDeviceInfo& device)
{
    bool isWiiRemoteProduct = device.productId == WiiRemoteProductId || device.productId == WiiRemotePlusProductId;
    return device.vendorId == NintendoVendorId && isWiiRemoteProduct;
}

std::optional<JoyConSide> JoyConSideOf(const Platform::HidDeviceInfo& device)
{
    if (device.vendorId != NintendoVendorId)
        return std::nullopt;

    if (device.productId == LeftJoyConProductId)
        return JoyConSide::Left;

    if (device.productId == RightJoyConProductId)
        return JoyConSide::Right;

    return std::nullopt;
}

std::string KindText(DeviceKind kind)
{
    switch (kind)
    {
    case DeviceKind::Remote:
        return "connected";
    case DeviceKind::RemoteWithNunchuk:
        return "connected, with a Nunchuk";
    case DeviceKind::RemoteWithClassicController:
        return "connected, with a Classic Controller";
    default:
        return "not connected";
    }
}

}

RealControllers::RealControllers(Platform::InputDeviceAccess& deviceAccess)
    : devices(deviceAccess)
{
}

void RealControllers::Update(double nowInSeconds)
{
    double elapsedSeconds = hasSearched ? nowInSeconds - now : 0;
    now = nowInSeconds;

    bool isSearchDue = !hasSearched || now - lastSearchTime >= SecondsBetweenSearches;
    if (isSearchDue)
    {
        hasSearched = true;
        lastSearchTime = now;
        FindNewDevices();
    }

    PollDevices();
    ForgetGoneDevices();
    PairJoyCons(elapsedSeconds);
    ReadGamepads(elapsedSeconds, isSearchDue);
    AssignChannels();
}

RemoteSource* RealControllers::SourceOfChannel(int channel) const
{
    if (channel < 0 || channel >= RemoteChannelCount)
        return nullptr;

    return channels[static_cast<std::size_t>(channel)];
}

void RealControllers::SetCameraSensitivity(int level)
{
    cameraSensitivity = level;
    for (OpenWiiRemote& open : wiiRemotes)
        open.remote->SetCameraSensitivity(level);
}

void RealControllers::SetControllerSettings(const ControllerSettings& newSettings)
{
    settings = newSettings;

    for (GamepadRemote& gamepad : gamepads)
    {
        gamepad.SetButtonMap(settings.gamepadMap);
        gamepad.SetPointerStick(settings.gamepadPointerStick);
        gamepad.SetPointerSpeed(settings.pointerSpeed);
    }

    for (std::unique_ptr<JoyConRemote>& joyConRemote : joyConRemotes)
    {
        joyConRemote->SetPointer(settings.joyConPointer);
        joyConRemote->SetPointerSpeed(settings.pointerSpeed);
    }
}

std::vector<std::string> RealControllers::DescribeDevices() const
{
    std::vector<std::string> lines;
    for (const OpenWiiRemote& open : wiiRemotes)
        lines.push_back(std::format("Wii Remote: {}{}", KindText(open.remote->Probe()), ChannelText(open.remote.get())));

    for (const std::unique_ptr<JoyConRemote>& joyConRemote : joyConRemotes)
    {
        std::string name = "Joy-Con pair";
        if (joyConRemote->Left() == nullptr)
            name = "Right Joy-Con";
        if (joyConRemote->Right() == nullptr)
            name = "Left Joy-Con";

        lines.push_back(std::format("{}: {}{}", name, KindText(joyConRemote->Probe()), ChannelText(joyConRemote.get())));
    }

    for (std::size_t slot = 0; slot < gamepads.size(); ++slot)
    {
        if (!isGamepadInSlot[slot])
            continue;

        const GamepadRemote& gamepad = gamepads[slot];
        lines.push_back(std::format("Gamepad in slot {}: {}{}", slot + 1, KindText(gamepad.Probe()), ChannelText(&gamepad)));
    }

    return lines;
}

void RealControllers::FindNewDevices()
{
    for (const Platform::HidDeviceInfo& device : devices.ListHidDevices())
    {
        if (IsOpen(device.path))
            continue;

        bool isWiiRemote = IsWiiRemote(device);
        std::optional<JoyConSide> joyConSide = JoyConSideOf(device);
        if (!isWiiRemote && !joyConSide)
            continue;

        std::unique_ptr<Platform::HidConnection> connection = devices.OpenHidDevice(device);
        if (!connection)
            continue;

        if (isWiiRemote)
        {
            auto remote = std::make_unique<WiiRemote>(std::move(connection));
            remote->SetCameraSensitivity(cameraSensitivity);
            wiiRemotes.push_back({device.path, std::move(remote)});
        }
        else
        {
            joyCons.push_back({device.path, std::make_unique<JoyCon>(std::move(connection), *joyConSide)});
        }
    }
}

bool RealControllers::IsOpen(const std::wstring& path) const
{
    for (const OpenWiiRemote& open : wiiRemotes)
    {
        if (open.path == path)
            return true;
    }

    for (const OpenJoyCon& open : joyCons)
    {
        if (open.path == path)
            return true;
    }

    return false;
}

void RealControllers::PollDevices()
{
    for (OpenWiiRemote& open : wiiRemotes)
        open.remote->Poll(now);

    for (OpenJoyCon& open : joyCons)
        open.joyCon->Poll(now);
}

// A Joy-Con that went takes the remote it was part of with it; the other Joy-Con of a pair is paired anew.
void RealControllers::ForgetGoneDevices()
{
    std::erase_if(wiiRemotes,
        [this](const OpenWiiRemote& open)
        {
            if (open.remote->IsPresent())
                return false;

            ReleaseChannelOf(open.remote.get());
            return true;
        });

    for (const OpenJoyCon& open : joyCons)
    {
        if (open.joyCon->IsPresent())
            continue;

        JoyCon* gone = open.joyCon.get();
        std::erase_if(joyConRemotes,
            [this, gone](const std::unique_ptr<JoyConRemote>& joyConRemote)
            {
                bool usesGone = joyConRemote->Right() == gone || joyConRemote->Left() == gone;
                if (usesGone)
                    ReleaseChannelOf(joyConRemote.get());
                return usesGone;
            });
    }

    std::erase_if(joyCons, [](const OpenJoyCon& open) { return !open.joyCon->IsPresent(); });
}

// The first right Joy-Con goes with the first left one, and so on; the ones left over are remotes alone.
void RealControllers::PairJoyCons(double elapsedSeconds)
{
    std::vector<JoyCon*> rights;
    std::vector<JoyCon*> lefts;
    for (const OpenJoyCon& open : joyCons)
    {
        if (open.joyCon->Side() == JoyConSide::Right)
            rights.push_back(open.joyCon.get());
        else
            lefts.push_back(open.joyCon.get());
    }

    std::vector<std::pair<JoyCon*, JoyCon*>> pairs;
    std::size_t pairCount = std::max(rights.size(), lefts.size());
    for (std::size_t index = 0; index < pairCount; ++index)
    {
        JoyCon* right = index < rights.size() ? rights[index] : nullptr;
        JoyCon* left = index < lefts.size() ? lefts[index] : nullptr;
        pairs.push_back({right, left});
    }

    std::erase_if(joyConRemotes,
        [this, &pairs](const std::unique_ptr<JoyConRemote>& joyConRemote)
        {
            auto found = std::find(pairs.begin(), pairs.end(), std::make_pair(joyConRemote->Right(), joyConRemote->Left()));
            if (found != pairs.end())
            {
                pairs.erase(found);
                return false;
            }

            ReleaseChannelOf(joyConRemote.get());
            return true;
        });

    for (const auto& [right, left] : pairs)
    {
        auto joyConRemote = std::make_unique<JoyConRemote>(right, left);
        joyConRemote->SetPointer(settings.joyConPointer);
        joyConRemote->SetPointerSpeed(settings.pointerSpeed);
        joyConRemotes.push_back(std::move(joyConRemote));
    }

    for (std::unique_ptr<JoyConRemote>& joyConRemote : joyConRemotes)
        joyConRemote->Update(elapsedSeconds);
}

// Asking XInput for an empty slot is slow, so an empty slot is asked only when the devices are searched.
void RealControllers::ReadGamepads(double elapsedSeconds, bool isSearchDue)
{
    for (std::size_t slot = 0; slot < gamepads.size(); ++slot)
    {
        if (!isGamepadInSlot[slot] && !isSearchDue)
            continue;

        int slotNumber = static_cast<int>(slot);
        std::optional<Platform::GamepadReading> reading = devices.ReadGamepad(slotNumber);
        isGamepadInSlot[slot] = reading.has_value();
        gamepads[slot].Update(reading, elapsedSeconds);

        bool shouldRumble = reading && gamepads[slot].IsRumbling();
        if (shouldRumble != isGamepadRumbling[slot])
        {
            isGamepadRumbling[slot] = shouldRumble;
            devices.SetGamepadRumble(slotNumber, shouldRumble);
        }
    }
}

void RealControllers::AssignChannels()
{
    for (RemoteSource*& source : channels)
    {
        if (source != nullptr && source->Probe() == DeviceKind::None)
            source = nullptr;
    }

    for (RemoteSource* remote : AllRemotes())
    {
        bool isOnChannel = std::find(channels.begin(), channels.end(), remote) != channels.end();
        if (isOnChannel || remote->Probe() == DeviceKind::None)
            continue;

        auto freeChannel = std::find(channels.begin(), channels.end(), nullptr);
        if (freeChannel == channels.end())
            return;

        *freeChannel = remote;
    }
}

void RealControllers::ReleaseChannelOf(const RemoteSource* source)
{
    for (RemoteSource*& channelSource : channels)
    {
        if (channelSource == source)
            channelSource = nullptr;
    }
}

std::vector<RemoteSource*> RealControllers::AllRemotes()
{
    std::vector<RemoteSource*> remotes;
    for (OpenWiiRemote& open : wiiRemotes)
        remotes.push_back(open.remote.get());

    for (std::unique_ptr<JoyConRemote>& joyConRemote : joyConRemotes)
        remotes.push_back(joyConRemote.get());

    for (GamepadRemote& gamepad : gamepads)
        remotes.push_back(&gamepad);

    return remotes;
}

std::string RealControllers::ChannelText(const RemoteSource* source) const
{
    for (std::size_t channel = 0; channel < channels.size(); ++channel)
    {
        if (channels[channel] == source)
            return std::format(", Wii Remote {}", channel + 1);
    }

    return "";
}

}
