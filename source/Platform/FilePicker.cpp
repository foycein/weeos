#include "Platform/FilePicker.h"

#include <objbase.h>
#include <shobjidl.h>

#include <thread>

namespace Platform
{

namespace
{

// Releases a COM object when it goes out of scope.
template <typename Interface>
class ComObject
{
public:
    ComObject() = default;
    ~ComObject()
    {
        if (object != nullptr)
            object->Release();
    }

    ComObject(const ComObject&) = delete;
    ComObject& operator=(const ComObject&) = delete;

    Interface** Address()
    {
        return &object;
    }

    Interface* operator->() const
    {
        return object;
    }

private:
    Interface* object = nullptr;
};

std::optional<std::filesystem::path> ShowPicker(const std::wstring& title, const std::wstring& fileName)
{
    ComObject<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.Address()))))
        return std::nullopt;

    COMDLG_FILTERSPEC filter = {fileName.c_str(), fileName.c_str()};
    dialog->SetFileTypes(1, &filter);
    dialog->SetTitle(title.c_str());
    dialog->SetOptions(FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM);

    if (FAILED(dialog->Show(nullptr)))
        return std::nullopt;

    ComObject<IShellItem> item;
    if (FAILED(dialog->GetResult(item.Address())))
        return std::nullopt;

    PWSTR path = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
        return std::nullopt;

    std::filesystem::path pickedPath(path);
    CoTaskMemFree(path);
    return pickedPath;
}

}

// The picker needs a thread of its own with a single-threaded COM apartment: the sound output of the program uses the
// multi-threaded one on the main thread. It has no owner window, because its owner would be blocked in another thread.
std::optional<std::filesystem::path> PickFile(const std::wstring& title, const std::wstring& fileName)
{
    std::optional<std::filesystem::path> pickedPath;

    std::thread pickerThread([&]() {
        if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
            return;

        pickedPath = ShowPicker(title, fileName);
        CoUninitialize();
    });
    pickerThread.join();

    return pickedPath;
}

}
