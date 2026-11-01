#include "Sound/PlaybackDevices.h"

#include <Windows.h>

#include <mmdeviceapi.h>

// This header needs the property key macros that mmdeviceapi.h brings, so it comes after it.
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

namespace Sound
{

namespace
{

using Microsoft::WRL::ComPtr;

std::u16string FriendlyNameOf(IMMDevice& device)
{
    ComPtr<IPropertyStore> properties;
    if (FAILED(device.OpenPropertyStore(STGM_READ, &properties)))
        return {};

    PROPVARIANT name;
    PropVariantInit(&name);
    std::u16string text;
    if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &name)) && name.vt == VT_LPWSTR)
        text = reinterpret_cast<const char16_t*>(name.pwszVal);

    PropVariantClear(&name);
    return text;
}

std::vector<std::u16string> ListWithCom()
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator))))
        return {};

    ComPtr<IMMDeviceCollection> devices;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices)))
        return {};

    UINT deviceCount = 0;
    devices->GetCount(&deviceCount);

    std::vector<std::u16string> names;
    for (UINT deviceNumber = 0; deviceNumber < deviceCount; ++deviceNumber)
    {
        ComPtr<IMMDevice> device;
        if (FAILED(devices->Item(deviceNumber, &device)))
            continue;

        std::u16string name = FriendlyNameOf(*device.Get());
        if (!name.empty())
            names.push_back(name);
    }

    return names;
}

}

std::vector<std::u16string> ListPlaybackDeviceNames()
{
    HRESULT started = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::vector<std::u16string> names = ListWithCom();

    // A thread that has COM already in another mode keeps it; only a start that worked is ended again.
    if (SUCCEEDED(started))
        CoUninitialize();

    return names;
}

}
