//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Module.h"
#include "UIInputOwnership.h"
#include "UIRootWidget.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace carrot::ui {
    class ui_focus_scope_t;
    class ui_widget_t;

    enum class ui_navigation_action_t : uint8_t
    {
        move_up,
        move_down,
        move_left,
        move_right,
        focus_next,
        focus_previous,
        accept,
        cancel,
    };

    enum class ui_feedback_event_t : uint8_t
    {
        focus_move,
        accept,
        cancel,
    };

    class ui_module_t final : public core::module_t
    {
    public:
        CARROT_MODULE_NAME("UI")
        using ui_feedback_callback_t = std::function<void(ui_feedback_event_t)>;

        ui_module_t() = default;
        ~ui_module_t() override = default;

        void init() override;
        void shutdown() override;

        void update(float delta_time) noexcept override;

        [[nodiscard]] ui_root_widget_t* get_root() noexcept { return _root.get(); }
        [[nodiscard]] const ui_root_widget_t* get_root() const noexcept { return _root.get(); }
        [[nodiscard]] ui_widget_t* get_focused_widget() noexcept { return _focused_widget; }
        [[nodiscard]] const ui_widget_t* get_focused_widget() const noexcept { return _focused_widget; }
        [[nodiscard]] bool is_focus_looping_enabled() const noexcept { return _focus_looping_enabled; }
        [[nodiscard]] const std::vector<std::string>& get_debug_navigation_events() const noexcept { return _debug_navigation_events; }
        [[nodiscard]] ui_input_ownership_mode_t get_input_ownership_mode() const noexcept { return _input_ownership_mode; }
        [[nodiscard]] ui_input_ownership_mode_t get_effective_input_ownership_mode() const noexcept;

        void set_root(std::unique_ptr<ui_root_widget_t> root) noexcept;
        void clear_root() noexcept;
        void set_focus_looping_enabled(bool enabled) noexcept { _focus_looping_enabled = enabled; }
        void set_input_ownership_mode(ui_input_ownership_mode_t mode) noexcept { _input_ownership_mode = mode; }
        void set_runtime_input_ownership_override(ui_input_ownership_mode_t mode) noexcept { _runtime_input_ownership_override = mode; }
        void clear_runtime_input_ownership_override() noexcept { _runtime_input_ownership_override.reset(); }
        void set_feedback_callback(ui_feedback_callback_t callback) noexcept { _feedback_callback = std::move(callback); }
        void clear_feedback_callback() noexcept { _feedback_callback = nullptr; }
        bool dispatch_navigation_action(ui_navigation_action_t action) noexcept;
        bool dispatch_navigation_action(std::string_view action_name) noexcept;
        bool set_focus(ui_widget_t* widget) noexcept;
        void clear_focus() noexcept;
        bool focus_first() noexcept;
        bool focus_next() noexcept;
        bool focus_previous() noexcept;
        bool navigate(ui_navigation_direction_t direction) noexcept;
        bool handle_navigation_action(ui_navigation_action_t action) noexcept;
        bool handle_navigation_action(std::string_view action_name) noexcept;

    private:
        struct focus_scope_runtime_t
        {
            ui_widget_t* restore_focus_target{ nullptr };
            bool was_visible{ false };
        };

        [[nodiscard]] bool is_widget_in_tree(const ui_widget_t* widget) const noexcept;
        [[nodiscard]] bool is_focusable_widget(const ui_widget_t* widget) const noexcept;
        [[nodiscard]] bool is_widget_visible_in_hierarchy(const ui_widget_t* widget) const noexcept;
        [[nodiscard]] bool is_widget_descendant_of(const ui_widget_t* widget, const ui_widget_t* ancestor) const noexcept;
        [[nodiscard]] ui_focus_scope_t* find_nearest_focus_scope_ancestor(ui_widget_t* widget) const noexcept;
        [[nodiscard]] const ui_focus_scope_t* find_nearest_focus_scope_ancestor(const ui_widget_t* widget) const noexcept;
        [[nodiscard]] std::vector<ui_focus_scope_t*> gather_visible_focus_scopes() const;
        [[nodiscard]] ui_widget_t* resolve_focus_target_for_scope(ui_focus_scope_t& scope) noexcept;
        bool activate_focus_scope(ui_focus_scope_t& scope) noexcept;
        void deactivate_focus_scope(ui_focus_scope_t& scope) noexcept;
        void reconcile_focus_scopes() noexcept;
        void gather_focusable_widgets(const ui_widget_t& widget, std::vector<ui_widget_t*>& out) const;
        void gather_focusable_widgets_in_subtree(const ui_widget_t& widget, std::vector<ui_widget_t*>& out) const;
        [[nodiscard]] std::vector<ui_widget_t*> gather_focusable_widgets() const;
        [[nodiscard]] ui_input_ownership_mode_t resolve_effective_input_ownership_mode() const noexcept;
        [[nodiscard]] static const char* input_ownership_mode_to_string(ui_input_ownership_mode_t mode) noexcept;
        [[nodiscard]] bool try_parse_navigation_action(std::string_view action_name, ui_navigation_action_t& out_action) const noexcept;
        static std::string navigation_action_to_string(ui_navigation_action_t action);
        void push_debug_navigation_event(std::string event_text);
        void emit_feedback_event(ui_feedback_event_t event) const noexcept;

        std::unique_ptr<ui_root_widget_t> _root;
        ui_widget_t* _focused_widget{ nullptr };
        ui_focus_scope_t* _active_focus_scope{ nullptr };
        bool _focus_looping_enabled{ true };
        ui_input_ownership_mode_t _input_ownership_mode{ ui_input_ownership_mode_t::ui_priority };
        std::optional<ui_input_ownership_mode_t> _runtime_input_ownership_override;
        ui_feedback_callback_t _feedback_callback;
        std::vector<std::string> _debug_navigation_events;
        std::unordered_map<ui_focus_scope_t*, focus_scope_runtime_t> _focus_scope_runtime;
        std::unordered_set<ui_focus_scope_t*> _visible_focus_scopes;
    };
} // namespace carrot::ui
