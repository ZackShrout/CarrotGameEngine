//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "UI/UIInputOwnership.h"
#include "UI/UIWidget.h"

namespace carrot::ui {
    enum class ui_focus_on_show_policy_t : uint8_t
    {
        first,
        last,
        explicit_target
    };

    class ui_focus_scope_t final : public ui_widget_t
    {
    public:
        [[nodiscard]] std::string_view get_debug_name() const noexcept override { return "ui_focus_scope_t"; }

        [[nodiscard]] bool is_focus_trap_enabled() const noexcept { return _focus_trap_enabled; }
        void set_focus_trap_enabled(const bool enabled) noexcept { _focus_trap_enabled = enabled; }

        [[nodiscard]] ui_focus_on_show_policy_t get_focus_on_show_policy() const noexcept { return _focus_on_show_policy; }
        void set_focus_on_show_policy(const ui_focus_on_show_policy_t policy) noexcept { _focus_on_show_policy = policy; }

        [[nodiscard]] ui_widget_t* get_explicit_focus_target() noexcept { return _explicit_focus_target; }
        [[nodiscard]] const ui_widget_t* get_explicit_focus_target() const noexcept { return _explicit_focus_target; }
        void set_explicit_focus_target(ui_widget_t* target) noexcept { _explicit_focus_target = target; }

        [[nodiscard]] ui_widget_t* get_last_focused_descendant() noexcept { return _last_focused_descendant; }
        [[nodiscard]] const ui_widget_t* get_last_focused_descendant() const noexcept { return _last_focused_descendant; }
        void set_last_focused_descendant(ui_widget_t* widget) noexcept { _last_focused_descendant = widget; }

        [[nodiscard]] bool has_input_ownership_override() const noexcept { return _input_ownership_override_enabled; }
        [[nodiscard]] ui_input_ownership_mode_t get_input_ownership_override() const noexcept { return _input_ownership_override_mode; }
        void set_input_ownership_override(const ui_input_ownership_mode_t mode) noexcept
        {
            _input_ownership_override_mode = mode;
            _input_ownership_override_enabled = true;
        }
        void clear_input_ownership_override() noexcept { _input_ownership_override_enabled = false; }

    private:
        bool _focus_trap_enabled{ true };
        ui_focus_on_show_policy_t _focus_on_show_policy{ ui_focus_on_show_policy_t::first };
        ui_widget_t* _explicit_focus_target{ nullptr };
        ui_widget_t* _last_focused_descendant{ nullptr };
        bool _input_ownership_override_enabled{ false };
        ui_input_ownership_mode_t _input_ownership_override_mode{ ui_input_ownership_mode_t::ui_priority };
    };
} // namespace carrot::ui
