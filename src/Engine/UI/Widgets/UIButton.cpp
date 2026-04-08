//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "UIButton.h"

namespace carrot::ui {
    ui_button_t::ui_button_t(std::string label) noexcept : _label(std::move(label))
    {
        set_focusable(true);
        set_desired_size({ 320.f, 48.f });
        set_min_size({ 200.f, 32.f });
    }

    void ui_button_t::set_label(std::string label) noexcept
    {
        if (_label == label)
            return;

        _label = std::move(label);
    }

    std::string_view ui_button_t::get_debug_name() const noexcept
    {
        return _label;
    }

    bool ui_button_t::get_debug_visual_style(ui_debug_visual_style_t& out) const noexcept
    {
        if (!can_receive_focus())
            return false;

        out.fill_color = 0x3D1F1Fu;
        out.focused_fill_color = 0x7A5230u;
        out.border_color = 0xFF7A7A7Au;
        out.focused_border_color = 0xFFFFD86Bu;
        out.border_thickness = 2.f;
        return true;
    }

    void ui_button_t::on_focus_gained() noexcept
    {
        if (_on_focused)
            _on_focused();
    }

    void ui_button_t::on_focus_lost() noexcept
    {
        if (_on_focus_lost)
            _on_focus_lost();
    }

    bool ui_button_t::on_ui_accept() noexcept
    {
        if (_on_pressed)
            _on_pressed();

        return true;
    }

    bool ui_button_t::on_ui_cancel() noexcept
    {
        if (_on_canceled)
            _on_canceled();

        return true;
    }
} // namespace carrot::ui
