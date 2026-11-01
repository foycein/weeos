#include "Platform/InputDevices.h"

#include <windows.h>

#include <hidsdi.h>
#include <setupapi.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace Platform
{

namespace
{

// Windows keeps this many reports of a device until they are taken. A Wii Remote sends about 100 in a second.
constexpr ULONG InputReportsKeptByWindows = 128;

// A device that does not take a report in this time is not there, for example an empty place of a Dolphin Bar.
constexpr DWORD LongestWriteInMilliseconds = 1000;

// Reports that wait for a device that takes none are dropped beyond this many.
constexpr std::size_t MostWaitingReports = 64;

// The longest product name a device can give is 126 characters and the end mark (hidsdi.h).
constexpr ULONG ProductNameBufferBytes = 254;

class DeviceHandle
{
public:
    explicit DeviceHandle(HANDLE openedHandle)
        : handle(openedHandle)
    {
    }

    ~DeviceHandle()
    {
        if (IsValid())
            CloseHandle(handle);
    }

    DeviceHandle(const DeviceHandle&) = delete;
    DeviceHandle& operator=(const DeviceHandle&) = delete;

    bool IsValid() const
    {
        return handle != INVALID_HANDLE_VALUE;
    }

    HANDLE Get() const
    {
        return handle;
    }

private:
    HANDLE handle;
};

std::wstring ProductNameOf(HANDLE device)
{
    wchar_t name[ProductNameBufferBytes / sizeof(wchar_t)] = {};
    if (!HidD_GetProductString(device, name, ProductNameBufferBytes))
        return {};

    return name;
}

// Opens the device without the right to read or write, which every device allows, to ask what it is.
std::optional<HidDeviceInfo> DescribeDevice(const std::wstring& path)
{
    DeviceHandle device(CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
    if (!device.IsValid())
        return std::nullopt;

    HIDD_ATTRIBUTES attributes = {};
    attributes.Size = sizeof(attributes);
    if (!HidD_GetAttributes(device.Get(), &attributes))
        return std::nullopt;

    HidDeviceInfo info;
    info.path = path;
    info.vendorId = attributes.VendorID;
    info.productId = attributes.ProductID;
    info.productName = ProductNameOf(device.Get());
    return info;
}

std::optional<std::wstring> InterfacePath(HDEVINFO deviceSet, SP_DEVICE_INTERFACE_DATA& interfaceData)
{
    DWORD requiredBytes = 0;
    SetupDiGetDeviceInterfaceDetailW(deviceSet, &interfaceData, nullptr, 0, &requiredBytes, nullptr);
    if (requiredBytes == 0)
        return std::nullopt;

    std::vector<std::byte> buffer(requiredBytes);
    auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    if (!SetupDiGetDeviceInterfaceDetailW(deviceSet, &interfaceData, detail, requiredBytes, nullptr, nullptr))
        return std::nullopt;

    return std::wstring(detail->DevicePath);
}

// Sends the reports one after the other on a thread of its own, so that a device that is slow to take one never holds up the program.
// Reads are not waited for either: one read is always under way, and TakeReport looks whether it has finished.
class WindowsHidConnection final : public HidConnection
{
public:
    WindowsHidConnection(HANDLE openedDevice, std::size_t inputReportBytes, std::size_t outputReportBytes)
        : device(openedDevice)
        , outputLength(outputReportBytes)
        , readBuffer(inputReportBytes)
    {
        readOverlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        writeOverlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        StartRead();
        writer = std::thread([this] { SendWaitingReports(); });
    }

    ~WindowsHidConnection() override
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            isClosing = true;
        }
        reportWaiting.notify_one();
        CancelIoEx(device.Get(), &writeOverlapped);
        writer.join();

        if (isReadUnderWay)
            FinishCancelledRead();

        CloseHandle(readOverlapped.hEvent);
        CloseHandle(writeOverlapped.hEvent);
    }

    void SendReport(std::span<const std::uint8_t> report) override
    {
        std::vector<std::uint8_t> padded(outputLength, 0);
        std::copy_n(report.begin(), std::min(report.size(), outputLength), padded.begin());

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (waitingReports.size() >= MostWaitingReports)
                return;
            waitingReports.push_back(std::move(padded));
        }
        reportWaiting.notify_one();
    }

    std::optional<std::vector<std::uint8_t>> TakeReport() override
    {
        if (!isOpen || !isReadUnderWay)
            return std::nullopt;

        if (!HasOverlappedIoCompleted(&readOverlapped))
            return std::nullopt;

        DWORD readBytes = 0;
        bool hasSucceeded = GetOverlappedResult(device.Get(), &readOverlapped, &readBytes, FALSE);
        isReadUnderWay = false;
        if (!hasSucceeded)
        {
            isOpen = false;
            return std::nullopt;
        }

        std::vector<std::uint8_t> report(readBuffer.begin(), readBuffer.begin() + readBytes);
        StartRead();
        return report;
    }

    bool IsOpen() const override
    {
        return isOpen;
    }

private:
    void StartRead()
    {
        ResetEvent(readOverlapped.hEvent);
        DWORD bufferBytes = static_cast<DWORD>(readBuffer.size());
        bool hasStarted = ReadFile(device.Get(), readBuffer.data(), bufferBytes, nullptr, &readOverlapped) || GetLastError() == ERROR_IO_PENDING;
        if (!hasStarted)
        {
            isOpen = false;
            return;
        }

        isReadUnderWay = true;
    }

    void FinishCancelledRead()
    {
        CancelIoEx(device.Get(), &readOverlapped);
        DWORD ignoredBytes = 0;
        GetOverlappedResult(device.Get(), &readOverlapped, &ignoredBytes, TRUE);
    }

    void SendWaitingReports()
    {
        while (true)
        {
            std::vector<std::uint8_t> report;
            {
                std::unique_lock<std::mutex> lock(mutex);
                reportWaiting.wait(lock, [this] { return isClosing || !waitingReports.empty(); });
                if (isClosing)
                    return;

                report = std::move(waitingReports.front());
                waitingReports.pop_front();
            }

            Write(report);
        }
    }

    // A write the device does not finish in time is cancelled, and the report is lost.
    void Write(std::vector<std::uint8_t>& report)
    {
        ResetEvent(writeOverlapped.hEvent);
        DWORD reportBytes = static_cast<DWORD>(report.size());
        if (WriteFile(device.Get(), report.data(), reportBytes, nullptr, &writeOverlapped))
            return;

        if (GetLastError() != ERROR_IO_PENDING)
            return;

        if (WaitForSingleObject(writeOverlapped.hEvent, LongestWriteInMilliseconds) != WAIT_OBJECT_0)
            CancelIoEx(device.Get(), &writeOverlapped);

        DWORD writtenBytes = 0;
        GetOverlappedResult(device.Get(), &writeOverlapped, &writtenBytes, TRUE);
    }

    DeviceHandle device;
    std::size_t outputLength;

    std::vector<std::uint8_t> readBuffer;
    OVERLAPPED readOverlapped = {};
    bool isReadUnderWay = false;
    bool isOpen = true;

    OVERLAPPED writeOverlapped = {};
    std::mutex mutex;
    std::condition_variable reportWaiting;
    std::deque<std::vector<std::uint8_t>> waitingReports;
    bool isClosing = false;
    std::thread writer;
};

}

std::vector<HidDeviceInfo> WindowsInputDevices::ListHidDevices()
{
    GUID hidClass = {};
    HidD_GetHidGuid(&hidClass);

    HDEVINFO deviceSet = SetupDiGetClassDevsW(&hidClass, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceSet == INVALID_HANDLE_VALUE)
        return {};

    std::vector<HidDeviceInfo> devices;
    SP_DEVICE_INTERFACE_DATA interfaceData = {};
    interfaceData.cbSize = sizeof(interfaceData);
    for (DWORD index = 0; SetupDiEnumDeviceInterfaces(deviceSet, nullptr, &hidClass, index, &interfaceData); ++index)
    {
        std::optional<std::wstring> path = InterfacePath(deviceSet, interfaceData);
        if (!path)
            continue;

        std::optional<HidDeviceInfo> info = DescribeDevice(*path);
        if (info)
            devices.push_back(*info);
    }

    SetupDiDestroyDeviceInfoList(deviceSet);
    return devices;
}

std::unique_ptr<HidConnection> WindowsInputDevices::OpenHidDevice(const HidDeviceInfo& deviceInfo)
{
    HANDLE device = CreateFileW(deviceInfo.path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (device == INVALID_HANDLE_VALUE)
        return nullptr;

    PHIDP_PREPARSED_DATA preparsedData = nullptr;
    HIDP_CAPS capabilities = {};
    bool hasCapabilities = HidD_GetPreparsedData(device, &preparsedData) && HidP_GetCaps(preparsedData, &capabilities) == HIDP_STATUS_SUCCESS;
    if (preparsedData != nullptr)
        HidD_FreePreparsedData(preparsedData);

    if (!hasCapabilities || capabilities.InputReportByteLength == 0)
    {
        CloseHandle(device);
        return nullptr;
    }

    HidD_SetNumInputBuffers(device, InputReportsKeptByWindows);
    return std::make_unique<WindowsHidConnection>(device, capabilities.InputReportByteLength, capabilities.OutputReportByteLength);
}

}
