#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <unordered_set>

#include <Windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "pacdll.hpp"
#include "window.hpp"
#include "ppsspp_api.hpp"
#include "pac_api.hpp"

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

        if (minimized_state_) {
            ::WaitMessage();
            return;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderInterface();

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
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
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
        if (FAILED(dx_swapchain_->GetBuffer(
                0, IID_ID3D11Texture2D, reinterpret_cast<void **>(back_buffer.GetAddressOf())))) {
            throw std::runtime_error{"GetBuffer failed"};
        }

        if (FAILED(
                dx_devie_->CreateRenderTargetView(back_buffer.Get(), nullptr, dx_target_.ReleaseAndGetAddressOf()))) {
            throw std::runtime_error{"CreateRenderTargetView failed"};
        }
    }

    struct PacManagerState {
        u32 ptr_base = 0;
        u32 ptr_flags = 0;
        u32 num_flags = 0;
        u32 ptr_ints = 0;
        u32 num_ints = 0;
        u32 ptr_flts = 0;
        u32 num_flts = 0;
        u32 ptr_array = 0;
        u32 cap_array = 0;
        u32 sp_array = 0;

        auto InitFromEmuMemory() -> void {
            ptr_base = g_emu.ReadUncheckedU32(PacSingleton::base);

            if (ptr_base == 0) {
                return;
            }

            ptr_flags = g_emu.ReadUncheckedU32(ptr_base + PacSingleton::flags_p);
            num_flags = g_emu.ReadUncheckedU32(ptr_base + PacSingleton::flags_n);
            ptr_ints = g_emu.ReadUncheckedU32(ptr_base + PacSingleton::global_p_ints);
            num_ints = g_emu.ReadUncheckedU32(ptr_base + PacSingleton::global_n_ints);
            ptr_flts = g_emu.ReadUncheckedU32(ptr_base + PacSingleton::global_p_flts);
            num_flts = g_emu.ReadUncheckedU32(ptr_base + PacSingleton::global_n_flts);
            ptr_array = g_emu.ReadUncheckedU32(ptr_base + PacSingleton::array_p);
            cap_array = g_emu.ReadUncheckedU32(ptr_base + PacSingleton::array_cap);
            sp_array = g_emu.ReadUncheckedU32(ptr_base + PacSingleton::array_sp);
        }
    };

    auto RenderInterface() -> void {
        ImGuiViewport *vp = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);

        constexpr ImGuiWindowFlags kRootFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                                ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                                ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGui::Begin("root", nullptr, kRootFlags);

        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("main_dockspace");
        if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);

            ImGuiID pane_upper, pane_lower_left, pane_lower_right;
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.25f, &pane_upper, &pane_lower_left);
            ImGui::DockBuilderSplitNode(pane_lower_left, ImGuiDir_Left, 0.5f, &pane_lower_left, &pane_lower_right);

            ImGui::DockBuilderDockWindow("Manager", pane_upper);
            ImGui::DockBuilderDockWindow("Globals (int)", pane_lower_left);
            ImGui::DockBuilderDockWindow("Globals (float)", pane_lower_right);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::DockSpace(dockspace_id, ImVec2{0, 0}, ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();

        PacManagerState pac_mgr_state = {};
        pac_mgr_state.InitFromEmuMemory();

        if (ImGui::Begin("Manager")) {
            RenderPacManagerState(pac_mgr_state);
        }

        ImGui::End();

        if (ImGui::Begin("Globals (int)")) {
            RenderPacIntGlobals(pac_mgr_state);
        }

        ImGui::End();

        if (ImGui::Begin("Globals (float)")) {
            RenderPacFltGlobals(pac_mgr_state);
        }

        ImGui::End();
    }

    auto RenderPacManagerState(const PacManagerState &pac_state) -> void {
        if (pac_state.ptr_base == 0) {
            ImGui::TextUnformatted("not yet initialized");
            return;
        }

        if (ImGui::BeginTable(
                "pac_manager", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Field");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            auto fnPrintRow = [](const char *name, u32 value) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(name);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("0x%08x", value);
            };

            fnPrintRow("ptr_base", pac_state.ptr_base);
            fnPrintRow("ptr_flags", pac_state.ptr_flags);
            fnPrintRow("num_flags", pac_state.num_flags);
            fnPrintRow("ptr_ints", pac_state.ptr_ints);
            fnPrintRow("num_ints", pac_state.num_ints);
            fnPrintRow("ptr_flts", pac_state.ptr_flts);
            fnPrintRow("num_flts", pac_state.num_flts);
            fnPrintRow("ptr_array", pac_state.ptr_array);
            fnPrintRow("cap_array", pac_state.cap_array);
            fnPrintRow("sp_array", pac_state.sp_array);

            ImGui::EndTable();
        }
    }

    auto RenderPacIntGlobals(const PacManagerState &pac_state) -> void {
        if (pac_state.ptr_ints == 0) {
            ImGui::TextUnformatted("not yet initialized");
            return;
        }

        if (ImGui::BeginTable(
                "globals_ints", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            auto fnPrintRow = [](u32 offset, u32 value) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("0x%08x", offset);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("0x%08x", value);
            };

            for (u32 i = 0; i < pac_state.num_ints; ++i) {
                const u32 ptr = pac_state.ptr_ints + (i * sizeof(u32));
                const s32 val = std::bit_cast<s32>(g_emu.ReadUncheckedU32(ptr));

                fnPrintRow(ptr, val);
            }

            ImGui::EndTable();
        }
    }

    auto RenderPacFltGlobals(const PacManagerState &pac_state) -> void {
        if (pac_state.ptr_flts == 0) {
            ImGui::TextUnformatted("not yet initialized");
            return;
        }

        if (ImGui::BeginTable(
                "globals_ints", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            auto fnPrintRow = [](u32 offset, float value) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("0x%08x", offset);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%f", value);
            };

            for (u32 i = 0; i < pac_state.num_flts; ++i) {
                const u32 ptr = pac_state.ptr_flts + (i * 4);
                const float val = g_emu.ReadUncheckedFloat(ptr);

                fnPrintRow(ptr, val);
            }

            ImGui::EndTable();
        }
    }

    auto ResizeSwapchain(HWND hwnd) -> void {
        RECT client_rect = {};
        ::GetClientRect(hwnd, &client_rect);

        LONG width = client_rect.right - client_rect.left;
        LONG height = client_rect.bottom - client_rect.top;

        if (!dx_swapchain_ || width <= 0 || height <= 0) {
            minimized_state_ = true;
            return;
        }

        minimized_state_ = false;

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

    struct PacSingleton {
        // clang-format off
        static constexpr u32 base             = 0x08ab9020;
        static constexpr u32 base_mgr         = 0x10;
        static constexpr u32 base_flags       = base_mgr + 0x1a8;
        static constexpr u32 base_global      = base_mgr + 0x1b8;
        static constexpr u32 base_array       = base_mgr + 0x1d8;

        static constexpr u32 flags_p          = base_flags + 0x4;
        static constexpr u32 flags_n          = base_flags + 0x8;

        static constexpr u32 global_p_ints    = base_global + 0x08;
        static constexpr u32 global_n_ints    = base_global + 0x0c;
        static constexpr u32 global_p_flts    = base_global + 0x14;
        static constexpr u32 global_n_flts    = base_global + 0x18;

        static constexpr u32 array_p          = base_array + 0x04;
        static constexpr u32 array_cap        = base_array + 0x08;
        static constexpr u32 array_sp         = base_array + 0x10;
        // clang-format on
    };

    std::chrono::steady_clock::time_point start_time_;

    bool size_move_state_ = false;
    bool minimized_state_ = false;

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
                .window_name = L"PAC debugger",
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
            ::CloseHandle(g_ThreadStopEvent);
            ::CloseHandle(g_hDllInterfaceThread);
        }

        g_ThreadStopEvent = nullptr;
        g_hDllInterfaceThread = nullptr;
    }

    return TRUE;
}

auto Init(EmuFuncs &funcs) -> void {
    g_ThreadStopEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_hDllInterfaceThread =
        ::CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(ThreadEntry), nullptr, 0, nullptr);

    g_emu.LoadFuncs(funcs);
}

// TODO: move elsewhere
void FormatPacLine(char *buf, size_t len, const PacState &state, const PacPosition &pos) {
    if (pos.file_name.has_value()) {
        snprintf(
            buf, len, "PAC: %s (%.*s:0x%x) id %d blk %d", pos.instruction_name, STR_VIEW(*pos.file_name), pos.offset,
            state.ctx_id, state.blk);

    } else {
        snprintf(
            buf, len, "PAC: %s (unk_0x%x:0x%x) id %d blk %d", pos.instruction_name, pos.file_size, pos.offset,
            state.ctx_id, state.blk);
    }
}

void GenericDefaultPacLog(Log type, LogLevel level, const PacState &state, const PacPosition &pos) {
    char buf[1024];
    FormatPacLine(buf, sizeof(buf), state, pos);
    g_emu.EmuLog(type, level, buf);
}

void ErrorDefaultPacLog(Log type, const PacState &state, const PacPosition &pos) {
    GenericDefaultPacLog(type, LogLevel::LERROR, state, pos);
}
void WarnDefaultPacLog(Log type, const PacState &state, const PacPosition &pos) {
    GenericDefaultPacLog(type, LogLevel::LWARNING, state, pos);
}
void InfoDefaultPacLog(Log type, const PacState &state, const PacPosition &pos) {
    GenericDefaultPacLog(type, LogLevel::LINFO, state, pos);
}
void NoticeDefaultPacLog(Log type, const PacState &state, const PacPosition &pos) {
    GenericDefaultPacLog(type, LogLevel::LNOTICE, state, pos);
}
void DebugDefaultPacLog(Log type, const PacState &state, const PacPosition &pos) {
    GenericDefaultPacLog(type, LogLevel::LDEBUG, state, pos);
}
void VerboseDefaultPacLog(Log type, const PacState &state, const PacPosition &pos) {
    GenericDefaultPacLog(type, LogLevel::LVERBOSE, state, pos);
}

auto Hook(PacState &state, MIPSState &mips) -> void {
    const auto &top = state.top();

    // if (state.top().file_name == "bases") {
    //     g_emu.EmuLog(Log::Printf, LogLevel::LNOTICE, "Hi from the handler!");
    //     g_emu.EnterStepping(&mips, BreakReason::HLEDebugBreak, 0);
    // }

    // if (top.offset == 0x0 && top.file_name == "dlc_0001") {
    //     // Patch!
    //     g_emu.WriteUncheckedFloat(-700.0f, PAC_ADDR(0x6C0));
    //     g_emu.WriteUncheckedU32(0x0, PAC_ADDR(0x7E0));
    //     g_emu.WriteUncheckedFloat(-700.0f, PAC_ADDR(0x834));
    //     g_emu.WriteUncheckedU32(0x0, PAC_ADDR(0x954));
    //     g_emu.WriteUncheckedFloat(-700.0f, PAC_ADDR(0x9A8));
    //     g_emu.WriteUncheckedU32(0x0, PAC_ADDR(0xAC8));
    //     g_emu.WriteUncheckedFloat(-700.0f, PAC_ADDR(0xB1C));
    //     g_emu.WriteUncheckedU32(0x2, PAC_ADDR(0xC3C));
    //     g_emu.WriteUncheckedFloat(-700.0f, PAC_ADDR(0xC90));
    //     g_emu.WriteUncheckedU32(0x0, PAC_ADDR(0xDB0));

    //     g_emu.EmuLog(Log::Printf, LogLevel::LNOTICE, "Patched the 50 Cyclops DLC PAC!");
    //     return;
    // }

    // if (top.offset == 0xDB8 && top.file_name == "dlc_0001") {
    //     g_emu.EmuLog(Log::Printf, LogLevel::LNOTICE, "Death call for the cyclops executed!");
    //     g_emu.EnterStepping(&mips, BreakReason::HLEDebugBreak, 0);
    // }

    // if (top.get_class_id() == 0x3) {
    //     NoticeDefaultPacLog(Log::Printf, state, top);
    // } else {
    //     InfoDefaultPacLog(Log::Printf, state, top);
    // }

    // {
    //     static std::unordered_set<u32> should_log;
    //     const u8 class_id = top.get_class_id();
    //     if (class_id == 0x3) {
    //         should_log.emplace(state.ctx_id);
    //     }

    //     if (should_log.count(state.ctx_id) != 0) {
    //         if (class_id == 0x3) {
    //             NoticeDefaultPacLog(Log::Printf, state, top);
    //         } else {
    //             InfoDefaultPacLog(Log::Printf, state, top);
    //         }
    //     }
    // }

    static int log_ahead_count = 0;

    if (top.instruction_signature == 0x25002E00) {
        const u32 args_count = g_emu.ReadUncheckedU32(top.get_pc() + 0x8);
        if (args_count == 1) {
            const u32 flg_id = g_emu.ReadUncheckedU32(top.get_pc() + 0xC);
            if (flg_id == 0x397) {
                pac::enable_flag(state, flg_id);
                // log_ahead_count = 16;
            }
        }
    }
    if (top.offset == 0xD4 && top.file_name == "azito") {
        g_emu.EmuLog(Log::Printf, LogLevel::LNOTICE, "TFLAG_SYS_DEBUG_START_LOCK enabled");
    }
    // if (top.offset == 0x2D948 && top.file_name == "azito") {
    //     log_ahead_count = 16;
    // }

    if (log_ahead_count > 1) {
        --log_ahead_count;
        InfoDefaultPacLog(Log::Printf, state, top);
    } else if (log_ahead_count == 1) {
        --log_ahead_count;
        InfoDefaultPacLog(Log::Printf, state, top);
        g_emu.EmuLog(Log::Printf, LogLevel::LNOTICE, "--------------------------------------------");
    }
}

auto Term() -> void {
    ::SetEvent(g_ThreadStopEvent);
    ::OutputDebugStringW(L"stopping the window thread\n");
    ::WaitForSingleObject(g_hDllInterfaceThread, INFINITE);
    ::CloseHandle(g_ThreadStopEvent);
    ::CloseHandle(g_hDllInterfaceThread);

    g_ThreadStopEvent = nullptr;
    g_hDllInterfaceThread = nullptr;

    ::OutputDebugStringW(L"teardown complete\n");
}
