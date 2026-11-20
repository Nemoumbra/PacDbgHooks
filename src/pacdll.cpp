#include <array>
#include <memory>

#include <Windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include "pacdll.hpp"
#include "window.hpp"

using Microsoft::WRL::ComPtr;

namespace {

class Context final {
public:
    Context() {}

    Context(const Context &) = delete;
    auto operator=(const Context &) = delete;

    Context(Context &&) noexcept = default;
    auto operator=(Context &&) noexcept -> Context & = default;

    auto WndProc(util::WindowHandle &window, UINT msg, [[maybe_unused]] LPARAM l_param, [[maybe_unused]] WPARAM w_param)
        -> util::EventResult {
        switch (msg) {
        case WM_CREATE:

        case WM_CLOSE:
            window.DestroyWindow();
            break;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            break;

        default:
            break;
        }

        return util::EventResult::eDefault;
    }

private:
    auto InitDisplay(HWND hwnd) -> void {
        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount = 2;
        scd.BufferDesc.Width = 0;
        scd.BufferDesc.Height = 0;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferDesc.RefreshRate.Numerator = 60;
        scd.BufferDesc.RefreshRate.Denominator = 1;
        scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = hwnd;
        scd.SampleDesc.Count = 1;
        scd.SampleDesc.Quality = 0;
        scd.Windowed = TRUE;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        constexpr UINT device_flags = 0;
        constexpr std::array<D3D_FEATURE_LEVEL, 2> request_levels = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0,
        };

        D3D_FEATURE_LEVEL feature_level;

        if (FAILED(
                ::D3D11CreateDeviceAndSwapChain(
                    nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags, request_levels.data(),
                    static_cast<UINT>(request_levels.size()), D3D11_SDK_VERSION, &scd,
                    dx_swapchain_.ReleaseAndGetAddressOf(), dx_devie_.ReleaseAndGetAddressOf(), &feature_level,
                    dx_dev_ctx_.ReleaseAndGetAddressOf()))) {
            throw std::runtime_error{"D3D11CreateDeviceAndSwapChain failed"};
        }

        ComPtr<ID3D11Texture2D> back_buffer;
        if (FAILED(dx_swapchain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
            dx_devie_->CreateRenderTargetView(back_buffer.Get(), nullptr, dx_target_.ReleaseAndGetAddressOf());
        }
    }

    auto RenderDisplay() -> void {}

    auto DestroyDisplay() -> void {
        dx_target_.Reset();
        dx_swapchain_.Reset();
        dx_dev_ctx_.Reset();
        dx_devie_.Reset();
    }

    ComPtr<ID3D11Device> dx_devie_;
    ComPtr<ID3D11DeviceContext> dx_dev_ctx_;
    ComPtr<IDXGISwapChain> dx_swapchain_;
    ComPtr<ID3D11RenderTargetView> dx_target_;
};

HANDLE g_hDllInterfaceThread = nullptr;
HINSTANCE g_hCurrentModule = nullptr;

auto WINAPI ThreadEntry([[maybe_unused]] LPVOID lp_param) -> DWORD {
    std::unique_ptr<util::AppWindow<Context>> app = nullptr;

    try {
        app = std::make_unique<util::AppWindow<Context>>(
            g_hCurrentModule,
            util::AppWindow<Context>::Description{
                .window_name = L"hello world",
            },
            Context{});
    } catch (const std::exception &e) {
        ::MessageBoxA(NULL, e.what(), "fatal error", MB_ICONERROR | MB_OK);
    }

    MSG msg = {};
    for (bool running = true; running;) {
        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);

            if (msg.message == WM_QUIT) {
                running = false;
            }
        }
    }

    return 0;
}

} // namespace

auto WINAPI DllMain(HINSTANCE instance, DWORD dw_reason, [[maybe_unused]] LPVOID lp_reserved) -> BOOL {
    ::DisableThreadLibraryCalls(instance);
    g_hCurrentModule = instance;

    if (dw_reason == DLL_PROCESS_DETACH) {
        if (g_hDllInterfaceThread) {
            ::TerminateThread(g_hDllInterfaceThread, 0);
        }

        g_hDllInterfaceThread = nullptr;
    }

    return TRUE;
}

auto Init() -> void {
    ::CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(ThreadEntry), nullptr, NULL, nullptr);
}
