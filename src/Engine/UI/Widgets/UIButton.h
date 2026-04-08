//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "UI/UIWidget.h"

#include <functional>
#include <string>
#include <string_view>

namespace carrot::ui {
    class ui_button_t final : public ui_widget_t
    {
    public:
        using callback_t = std::function<void()>;

        explicit ui_button_t(std::string label = "Button") noexcept;

        [[nodiscard]] std::string_view get_label() const noexcept { return _label; }
        void set_label(std::string label) noexcept;

        void set_on_focused(callback_t callback) noexcept { _on_focused = std::move(callback); }
        void set_on_focus_lost(callback_t callback) noexcept { _on_focus_lost = std::move(callback); }
        void set_on_pressed(callback_t callback) noexcept { _on_pressed = std::move(callback); }
        void set_on_canceled(callback_t callback) noexcept { _on_canceled = std::move(callback); }

        [[nodiscard]] std::string_view get_debug_name() const noexcept override;
        [[nodiscard]] bool get_debug_visual_style(ui_debug_visual_style_t& out) const noexcept override;

    protected:
        void on_focus_gained() noexcept override;
        void on_focus_lost() noexcept override;
        [[nodiscard]] bool on_ui_accept() noexcept override;
        [[nodiscard]] bool on_ui_cancel() noexcept override;

    private:
        std::string _label;
        callback_t _on_focused;
        callback_t _on_focus_lost;
        callback_t _on_pressed;
        callback_t _on_canceled;
    };
} // namespace carrot::ui
