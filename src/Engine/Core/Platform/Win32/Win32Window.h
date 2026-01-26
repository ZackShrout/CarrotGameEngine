//
// Created by zshro on 1/25/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Platform/Window.h"

#include <Windows.h>

namespace carrot::core::platform {
    class win32_window_t final : public window_t
    {
    public:
        explicit win32_window_t(uint32_t width, uint32_t height, std::string_view title) noexcept;
        ~win32_window_t() noexcept override;

        void poll_events() noexcept override;
        void set_should_close(bool should_close) noexcept override;

        [[nodiscard]] bool should_close() const noexcept override { return _should_close; }
        [[nodiscard]] uint32_t get_width() const noexcept override { return _width; }
        [[nodiscard]] uint32_t get_height() const noexcept override { return _height; }
        [[nodiscard]] bool is_minimized() const noexcept override { return _is_minimized; }
        [[nodiscard]] native_window_handle_t get_native_handle() const noexcept override;

        [[nodiscard]] bool is_fullscreen() const noexcept override { return _is_fullscreen; }
        void set_fullscreen(bool fullscreen) noexcept override;

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

    private:
        LRESULT handle_message(UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

        HWND _hwnd{ nullptr };
        HINSTANCE _hinstance{ nullptr };
        uint32_t _width{ 0 };
        uint32_t _height{ 0 };
        bool _should_close{ false };
        bool _is_minimized{ false };
        bool _is_fullscreen{ false };
        bool _was_maximized{ false };
        DWORD _prev_style{ 0 };
        DWORD _prev_ex_style{ 0 };
        RECT _prev_window_rect{ };
        std::wstring _title;
        chlm::float2 _last_mouse_position{ 0.f, 0.f };
    };
} // namespace carrot::core::platform
