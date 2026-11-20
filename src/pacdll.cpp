#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>

#include <Windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "pacdll.hpp"
#include "window.hpp"

using Microsoft::WRL::ComPtr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

namespace {

class Context final {
public:
    Context() : start_time_{std::chrono::steady_clock::now()} {}

    Context(const Context &) = delete;
    auto operator=(const Context &) = delete;

    Context(Context &&) noexcept = default;
    auto operator=(Context &&) noexcept -> Context & = default;

    auto WndProc(util::WindowHandle &window, UINT msg, WPARAM w_param, LPARAM l_param) -> util::EventResult {
        if (ImGui_ImplWin32_WndProcHandler(window.GetWindowHandle(), msg, w_param, l_param)) {
            return util::EventResult::ePreventDefault;
        }

        switch (msg) {
        case WM_CREATE:
            InitDisplay(window.GetWindowHandle());
            break;

        case WM_ENTERSIZEMOVE:
            size_move_state_ = true;
            break;

        case WM_EXITSIZEMOVE:
            size_move_state_ = false;
            ResizeSwapchain(window.GetWindowHandle());
            break;

        case WM_SIZE:
            if (!size_move_state_) {
                ResizeSwapchain(window.GetWindowHandle());
            }

            break;

        case WM_CLOSE:
            DestroyDisplay();
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

    auto RenderDisplay() -> void {
        const auto elapsed = std::chrono::steady_clock::now() - start_time_;
        const double time = std::chrono::duration<double>(elapsed).count();

        if (!gui_context_.has_value()) {
            ::OutputDebugStringW(L"gui has already been destroyed\n");
            return;
        }

        ::OutputDebugStringW(L"display render display please please please please\n");

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        ImGui::EndFrame();
        ImGui::Render();

        const std::array<float, 4> clear_color = {
            static_cast<float>(std::sin(time)),
            static_cast<float>(std::cos(time)),
            static_cast<float>(std::sin(1.0 + 1.5 * time)),
            1.0f,
        };

        dx_dev_ctx_->OMSetRenderTargets(1, dx_target_.GetAddressOf(), nullptr);
        dx_dev_ctx_->ClearRenderTargetView(dx_target_.Get(), clear_color.data());

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        dx_swapchain_->Present(1, 0);
    }

private:
    struct ImGuiContext {
        ImGuiContext(HWND hwnd, ID3D11Device *dx_device, ID3D11DeviceContext *dx_dev_ctx) : has_context_{true} {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();

            ImGuiIO &io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.ConfigWindowsMoveFromTitleBarOnly = true;

            ImGui::StyleColorsDark();

            ImGui_ImplWin32_Init(hwnd);
            ImGui_ImplDX11_Init(dx_device, dx_dev_ctx);
        }

        ~ImGuiContext() {
            if (has_context_) {
                DestroyContext();
            }
        }

        ImGuiContext(const ImGuiContext &) = delete;
        auto operator=(const ImGuiContext &) = delete;

        ImGuiContext(ImGuiContext &&other) noexcept : has_context_{other.has_context_} { other.has_context_ = false; }

        auto operator=(ImGuiContext &&other) noexcept -> ImGuiContext & {
            if (this != &other) {
                has_context_ = other.has_context_;
                other.has_context_ = false;
            }

            return *this;
        }

    private:
        auto DestroyContext() -> void {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            has_context_ = false;
        }

        bool has_context_ = false;
    };

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

        InitTarget();
        gui_context_ = ImGuiContext{hwnd, dx_devie_.Get(), dx_dev_ctx_.Get()};
    }

    auto InitTarget() -> void {
        ComPtr<ID3D11Texture2D> back_buffer;
        if (FAILED(dx_swapchain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
            throw std::runtime_error{"GetBuffer failed"};
        }

        if (FAILED(
                dx_devie_->CreateRenderTargetView(back_buffer.Get(), nullptr, dx_target_.ReleaseAndGetAddressOf()))) {
            throw std::runtime_error{"CreateRenderTargetView failed"};
        }
    }

    auto ResizeSwapchain(HWND hwnd) -> void {
        RECT client_rect = {};
        ::GetClientRect(hwnd, &client_rect);

        LONG width = client_rect.right - client_rect.left;
        LONG height = client_rect.bottom - client_rect.top;

        if (!dx_swapchain_ || width <= 0 || height <= 0) {
            return;
        }

        ID3D11RenderTargetView *null_view = nullptr;
        dx_dev_ctx_->OMSetRenderTargets(1, &null_view, nullptr);

        dx_target_.Reset();
        dx_dev_ctx_->Flush();

        if (FAILED(dx_swapchain_->ResizeBuffers(
                0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0))) {
            ::DestroyWindow(hwnd);
            return;
        }

        try {
            InitTarget();
        } catch (const std::exception &) {
            ::DestroyWindow(hwnd);
            return;
        }
    }

    auto DestroyDisplay() -> void {
        gui_context_.reset();
        dx_target_.Reset();
        dx_swapchain_.Reset();
        dx_dev_ctx_.Reset();
        dx_devie_.Reset();
    }

    std::chrono::steady_clock::time_point start_time_;
    bool size_move_state_ = false;

    ComPtr<ID3D11Device> dx_devie_;
    ComPtr<ID3D11DeviceContext> dx_dev_ctx_;
    ComPtr<IDXGISwapChain> dx_swapchain_;
    ComPtr<ID3D11RenderTargetView> dx_target_;
    std::optional<ImGuiContext> gui_context_;
};

HANDLE g_hDllInterfaceThread = nullptr;
HANDLE g_ThreadStopEvent = nullptr;
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
    for (bool running = true; running && ::WaitForSingleObject(g_ThreadStopEvent, 0) != WAIT_OBJECT_0;) {
        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);

            if (msg.message == WM_QUIT) {
                running = false;
            }
        }

        app->GetContext().RenderDisplay();
    }

    return 0;
}

} // namespace

auto WINAPI DllMain(HINSTANCE instance, DWORD dw_reason, [[maybe_unused]] LPVOID lp_reserved) -> BOOL {
    ::DisableThreadLibraryCalls(instance);
    g_hCurrentModule = instance;

    if (dw_reason == DLL_PROCESS_DETACH) {
        if (g_hDllInterfaceThread) {
            ::SetEvent(g_ThreadStopEvent);
            ::WaitForSingleObject(g_hDllInterfaceThread, INFINITE);
            ::CloseHandle(g_ThreadStopEvent);
            ::CloseHandle(g_hDllInterfaceThread);
        }

        g_ThreadStopEvent = nullptr;
        g_hDllInterfaceThread = nullptr;
    }

    return TRUE;
}

auto Init() -> void {
    g_ThreadStopEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_hDllInterfaceThread =
        ::CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(ThreadEntry), nullptr, NULL, nullptr);
}
