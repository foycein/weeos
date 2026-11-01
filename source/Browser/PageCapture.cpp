#include "Browser/PageCapture.h"

#include "Browser/BrowserError.h"

#include <d3d11.h>
#include <dxgi.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <wrl/client.h>

#include <mutex>

namespace Browser
{

namespace
{

namespace Capture = winrt::Windows::Graphics::Capture;
namespace DirectX = winrt::Windows::Graphics::DirectX;
using Microsoft::WRL::ComPtr;

// Two buffers: Windows fills one while the capture copies the other.
constexpr int FramesInPool = 2;
constexpr DirectX::DirectXPixelFormat CapturedFormat = DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized;
constexpr std::uint32_t BytesPerCapturedPixel = 4;
constexpr std::size_t BlueByte = 0;
constexpr std::size_t GreenByte = 1;
constexpr std::size_t RedByte = 2;
constexpr std::uint8_t OpaqueAlpha = 0xFF;

ComPtr<ID3D11Device> MakeGraphicsDevice(ComPtr<ID3D11DeviceContext>& context)
{
    ComPtr<ID3D11Device> device;
    HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
        D3D11_SDK_VERSION, &device, nullptr, &context);
    if (FAILED(result))
        throw BrowserError("The screen capture of the page found no Direct3D 11 graphics card");

    return device;
}

DirectX::Direct3D11::IDirect3DDevice CaptureDeviceOf(ID3D11Device& device)
{
    ComPtr<IDXGIDevice> dxgiDevice;
    device.QueryInterface(IID_PPV_ARGS(&dxgiDevice));

    winrt::com_ptr<IInspectable> inspectable;
    if (FAILED(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put())))
        throw BrowserError("The screen capture of the page cannot use the graphics card");

    return inspectable.as<DirectX::Direct3D11::IDirect3DDevice>();
}

Capture::GraphicsCaptureItem CaptureItemOf(const winrt::Windows::UI::Composition::Visual& visual)
{
    try
    {
        return Capture::GraphicsCaptureItem::CreateFromVisual(visual);
    }
    catch (const winrt::hresult_error&)
    {
        throw BrowserError("Windows cannot capture the visual of the page");
    }
}

ComPtr<ID3D11Texture2D> TextureOf(const Capture::Direct3D11CaptureFrame& frame)
{
    auto access = frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();

    ComPtr<ID3D11Texture2D> texture;
    access->GetInterface(IID_PPV_ARGS(&texture));
    return texture;
}

// The captured pixels are blue, green, red and alpha. The page is opaque, so the alpha that Windows leaves in them is not used.
void CopyRowAsRgba(const std::uint8_t* blueGreenRedAlpha, Formats::Rgba8Pixel* row, std::uint32_t width)
{
    for (std::uint32_t x = 0; x < width; ++x)
    {
        const std::uint8_t* pixel = blueGreenRedAlpha + x * BytesPerCapturedPixel;
        row[x] = {pixel[RedByte], pixel[GreenByte], pixel[BlueByte], OpaqueAlpha};
    }
}

}

struct PageCapture::Parts
{
    PictureReceiver receiver;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> readableCopy;
    DirectX::Direct3D11::IDirect3DDevice captureDevice = nullptr;
    Capture::GraphicsCaptureItem item = nullptr;
    Capture::Direct3D11CaptureFramePool pool = nullptr;
    Capture::GraphicsCaptureSession session = nullptr;
    winrt::Windows::Graphics::SizeInt32 poolSize = {};
    std::mutex frameLock;

    void TakeFrame();
    Formats::RgbaImage ReadPixels(ID3D11Texture2D& texture, std::uint32_t width, std::uint32_t height);
    void MakeReadableCopy(const D3D11_TEXTURE2D_DESC& textureDescription);
};

PageCapture::PageCapture(const winrt::Windows::UI::Composition::Visual& visual, PictureReceiver receiver)
    : parts(std::make_unique<Parts>())
{
    parts->receiver = std::move(receiver);
    parts->device = MakeGraphicsDevice(parts->context);
    parts->captureDevice = CaptureDeviceOf(*parts->device.Get());
    parts->item = CaptureItemOf(visual);
    parts->poolSize = parts->item.Size();

    parts->pool = Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(parts->captureDevice, CapturedFormat, FramesInPool, parts->poolSize);
    parts->pool.FrameArrived([this](const Capture::Direct3D11CaptureFramePool&, const winrt::Windows::Foundation::IInspectable&) {
        parts->TakeFrame();
    });

    parts->session = parts->pool.CreateCaptureSession(parts->item);
    parts->session.IsCursorCaptureEnabled(false);
    parts->session.IsBorderRequired(false);
    parts->session.StartCapture();
}

PageCapture::~PageCapture()
{
    parts->session.Close();
    parts->pool.Close();

    std::lock_guard<std::mutex> waitForTheLastFrame(parts->frameLock);
}

void PageCapture::Parts::TakeFrame()
{
    std::lock_guard<std::mutex> oneFrameAtATime(frameLock);

    Capture::Direct3D11CaptureFrame frame = pool.TryGetNextFrame();
    if (!frame)
        return;

    // A window that changed its size gives pictures of the new size once the pool has buffers of that size.
    winrt::Windows::Graphics::SizeInt32 contentSize = frame.ContentSize();
    if (contentSize.Width != poolSize.Width || contentSize.Height != poolSize.Height)
    {
        poolSize = contentSize;
        pool.Recreate(captureDevice, CapturedFormat, FramesInPool, poolSize);
        return;
    }

    // This runs on a thread of Windows, where nothing may be thrown. A picture that cannot be read is left out; the next change brings the next.
    try
    {
        ComPtr<ID3D11Texture2D> texture = TextureOf(frame);
        std::uint32_t width = static_cast<std::uint32_t>(contentSize.Width);
        std::uint32_t height = static_cast<std::uint32_t>(contentSize.Height);
        receiver(ReadPixels(*texture.Get(), width, height));
    }
    catch (const BrowserError&)
    {
    }
    catch (const winrt::hresult_error&)
    {
    }
}

Formats::RgbaImage PageCapture::Parts::ReadPixels(ID3D11Texture2D& texture, std::uint32_t width, std::uint32_t height)
{
    D3D11_TEXTURE2D_DESC textureDescription = {};
    texture.GetDesc(&textureDescription);
    MakeReadableCopy(textureDescription);

    context->CopyResource(readableCopy.Get(), &texture);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(readableCopy.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        throw BrowserError("The captured picture of the page cannot be read");

    Formats::RgbaImage picture{width, height, std::vector<Formats::Rgba8Pixel>(static_cast<std::size_t>(width) * height)};
    const std::uint8_t* firstRow = static_cast<const std::uint8_t*>(mapped.pData);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        const std::uint8_t* capturedRow = firstRow + static_cast<std::size_t>(y) * mapped.RowPitch;
        Formats::Rgba8Pixel* pictureRow = &picture.pixels[static_cast<std::size_t>(y) * width];
        CopyRowAsRgba(capturedRow, pictureRow, width);
    }

    context->Unmap(readableCopy.Get(), 0);
    return picture;
}

// The picture of Windows lives on the graphics card where the processor cannot read it, so it is copied into a texture that it can.
void PageCapture::Parts::MakeReadableCopy(const D3D11_TEXTURE2D_DESC& textureDescription)
{
    if (readableCopy)
    {
        D3D11_TEXTURE2D_DESC copyDescription = {};
        readableCopy->GetDesc(&copyDescription);
        if (copyDescription.Width == textureDescription.Width && copyDescription.Height == textureDescription.Height)
            return;
    }

    D3D11_TEXTURE2D_DESC copyDescription = textureDescription;
    copyDescription.Usage = D3D11_USAGE_STAGING;
    copyDescription.BindFlags = 0;
    copyDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    copyDescription.MiscFlags = 0;

    readableCopy.Reset();
    if (FAILED(device->CreateTexture2D(&copyDescription, nullptr, &readableCopy)))
        throw BrowserError("The capture of the page has no memory for a picture");
}

}
