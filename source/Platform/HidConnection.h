#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Platform
{

// A device of the class "human interface device" that Windows knows: Wii Remotes over Bluetooth, the Wii Remotes of a Dolphin Bar,
// Joy-Cons, but also keyboards and mice. The numbers say who made it and what it is.
struct HidDeviceInfo
{
    // The name Windows gives the device. It opens the device.
    std::wstring path;

    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;

    // What the device calls itself, for example "Nintendo RVL-CNT-01". Empty when it says nothing.
    std::wstring productName;
};

// An open device that sends and takes reports. A report is a packet of bytes whose first byte is the number of the report.
// All the functions are called from one thread and never wait for the device.
class HidConnection
{
public:
    HidConnection() = default;
    HidConnection(const HidConnection&) = delete;
    HidConnection& operator=(const HidConnection&) = delete;
    virtual ~HidConnection() = default;

    // Puts the report in line to be sent, after the reports before it. A report the device does not take is lost.
    virtual void SendReport(std::span<const std::uint8_t> report) = 0;

    // The oldest report the device has sent that was not taken yet, or nothing.
    virtual std::optional<std::vector<std::uint8_t>> TakeReport() = 0;

    // False once the device has gone away. It never comes back on this connection.
    virtual bool IsOpen() const = 0;
};

}
