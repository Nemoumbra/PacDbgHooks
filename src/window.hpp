#pragma once
#include <concepts>
#include <cstdint>
#include <string>
#include <system_error>

#include <Windows.h>

namespace util {

enum class EventResult {
    eDefault,
    ePreventDefault,
};

class WindowHandle final {
public:
    WindowHandle(HWND hwnd) : hwnd_{hwnd} {}

    WindowHandle(const WindowHandle &) = delete;
    auto operator=(const WindowHandle &) = delete;

    WindowHandle(WindowHandle &&) noexcept = delete;
    auto operator=(WindowHandle &&) noexcept = delete;

    auto GetWindowHandle() const -> HWND { return hwnd_; }
    auto GetWindowHandle() -> HWND { return hwnd_; }

    auto CloseWindow() -> void { ::CloseWindow(hwnd_); }
    auto DestroyWindow() -> void { ::DestroyWindow(hwnd_); }

private:
    HWND hwnd_ = nullptr;
};

template <typename Ctx>
concept AppWindowContext = requires(Ctx &ctx, WindowHandle &win) {
    { ctx.WndProc(win, UINT{}, LPARAM{}, WPARAM{}) } -> std::convertible_to<EventResult>;
};

template <AppWindowContext Ctx> class AppWindow {
public:
    static constexpr std::wstring_view kClassName = L"pacdllwindow";

    struct Description {
        DWORD dw_style_ex = 0;
        DWORD dw_style = WS_OVERLAPPEDWINDOW;

        std::wstring window_name;

        uint32_t width = 640;
        uint32_t height = 480;
    };

    AppWindow(HINSTANCE instance, const Description &description, Ctx context) : context_{std::move(context)} {
        s_RegisterWindowClass(instance);

        ::CreateWindowExW(
            description.dw_style_ex, kClassName.data(), description.window_name.c_str(), description.dw_style,
            CW_USEDEFAULT, CW_USEDEFAULT, static_cast<int>(description.width), static_cast<int>(description.height),
            nullptr, nullptr, instance, reinterpret_cast<void *>(this));

        if (!hwnd_) {
            throw std::system_error{
                std::error_code{static_cast<int>(::GetLastError()), std::system_category()}, "CreateWindowExW"};
        }

        ::ShowWindow(hwnd_, SW_SHOW);
        ::UpdateWindow(hwnd_);
    }

    ~AppWindow() noexcept {
        if (hwnd_) {
            ::DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
    }

    AppWindow(const AppWindow &) = delete;
    auto operator=(const AppWindow &) = delete;

    AppWindow(AppWindow &&) noexcept = delete;
    auto operator=(AppWindow &&) noexcept = delete;

    auto GetWindowHandle() const -> HWND { return hwnd_; }
    auto GetWindowHandle() -> HWND { return hwnd_; }

private:
    class WindowClass final {
    public:
        WindowClass(HINSTANCE instance, std::wstring_view class_name) {
            LogInfoW(L"registering window class");

            const WNDCLASSEXW wcex = {
                .cbSize = sizeof(WNDCLASSEXW),
                .style = CS_HREDRAW | CS_VREDRAW,
                .lpfnWndProc = s_WndProc,
                .cbClsExtra = 0,
                .cbWndExtra = sizeof(AppWindow *),
                .hInstance = instance,
                .hIcon = ::LoadIcon(nullptr, IDI_APPLICATION),
                .hCursor = ::LoadCursor(nullptr, IDC_ARROW),
                .hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
                .lpszMenuName = nullptr,
                .lpszClassName = class_name.data(),
                .hIconSm = ::LoadIcon(nullptr, IDI_APPLICATION),
            };

            auto res = ::RegisterClassExW(&wcex);
            if (0 == res) {
                throw std::system_error{
                    std::error_code{static_cast<int>(::GetLastError()), std::system_category()}, "RegisterClassExW"};
            }
        }

        WindowClass(const WindowClass &) = delete;
        auto operator=(const WindowClass &) = delete;

        WindowClass(WindowClass &&) noexcept = delete;
        auto operator=(WindowClass &&) noexcept = delete;
    };

    static auto s_RegisterWindowClass(HINSTANCE instance) -> void {
        static WindowClass window_class{instance, kClassName};
    }

    static auto CALLBACK s_WndProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) -> LRESULT {
        AppWindow *window = nullptr;
        if (msg == WM_NCCREATE) {
            window = reinterpret_cast<AppWindow *>(reinterpret_cast<CREATESTRUCTW *>(l_param)->lpCreateParams);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        } else {
            window = reinterpret_cast<AppWindow *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        EventResult result = EventResult::eDefault;
        if (window) {
            WindowHandle handle{window->hwnd_};
            switch (msg) {
            case WM_NCCREATE:
                window->hwnd_ = hwnd;
                break;

            case WM_DESTROY:
                ::DestroyWindow(window->hwnd_);
                window->hwnd_ = nullptr;
                break;

            default:
                break;
            }

            result = window->context_.WndProc(handle, msg, l_param, w_param);
        }

        if (result == EventResult::eDefault) {
            return ::DefWindowProcW(hwnd, msg, w_param, l_param);
        } else {
            return 0;
        }
    }

    auto WndProc([[maybe_unused]] UINT msg, [[maybe_unused]] LPARAM l_param, [[maybe_unused]] WPARAM w_param)
        -> EventResult {
        return EventResult::eDefault;
    }

    Ctx context_;
    HWND hwnd_ = nullptr;
};

} // namespace util