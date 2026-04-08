//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "UIModule.h"
#include "Widgets/UIFocusScope.h"

namespace carrot::ui {
    namespace {
        constexpr size_t k_max_debug_navigation_events{ 8u };
    } // namespace

    // PUBLIC

    void ui_module_t::init()
    {
        if (_is_initialized) return;

        if (!_root)
            _root = std::make_unique<ui_root_widget_t>();

        _root->attach_to_tree();
        _focused_widget = nullptr;
        _active_focus_scope = nullptr;
        _focus_scope_runtime.clear();
        _visible_focus_scopes.clear();
        _debug_navigation_events.clear();

        _is_initialized = true;
    }

    void ui_module_t::shutdown()
    {
        if (!_is_initialized) return;

        if (_root)
            _root->detach_from_tree();

        _focused_widget = nullptr;
        _active_focus_scope = nullptr;
        _focus_scope_runtime.clear();
        _visible_focus_scopes.clear();
        _debug_navigation_events.clear();
        _root.reset();

        _is_initialized = false;
    }

    void ui_module_t::update(const float delta_time) noexcept
    {
        if (!_is_initialized || !_root) return;

        if (_focused_widget && !is_widget_in_tree(_focused_widget))
            _focused_widget = nullptr;
        if (_active_focus_scope && !is_widget_in_tree(_active_focus_scope))
            _active_focus_scope = nullptr;

        reconcile_focus_scopes();
        _root->tick_tree(delta_time);
    }

    void ui_module_t::set_root(std::unique_ptr<ui_root_widget_t> root) noexcept
    {
        if (!root) return;

        if (_root && _is_initialized)
            _root->detach_from_tree();

        if (_focused_widget)
            clear_focus();

        _root = std::move(root);
        _active_focus_scope = nullptr;
        _focus_scope_runtime.clear();
        _visible_focus_scopes.clear();
        _debug_navigation_events.clear();

        if (_is_initialized)
            _root->attach_to_tree();
    }

    void ui_module_t::clear_root() noexcept
    {
        if (_root && _is_initialized)
            _root->detach_from_tree();

        if (_focused_widget)
            clear_focus();

        _root = std::make_unique<ui_root_widget_t>();
        _active_focus_scope = nullptr;
        _focus_scope_runtime.clear();
        _visible_focus_scopes.clear();
        _debug_navigation_events.clear();

        if (_is_initialized)
            _root->attach_to_tree();
    }

    bool ui_module_t::set_focus(ui_widget_t* widget) noexcept
    {
        if (!widget)
        {
            clear_focus();
            return true;
        }

        if (!_root || !_is_initialized)
            return false;

        if (!is_widget_in_tree(widget) || !is_focusable_widget(widget))
            return false;

        if (_focused_widget == widget)
            return true;

        if (_focused_widget)
            _focused_widget->notify_focus_lost();

        _focused_widget = widget;
        _focused_widget->notify_focus_gained();

        for (ui_focus_scope_t* scope{ find_nearest_focus_scope_ancestor(_focused_widget) };
             scope;
             scope = find_nearest_focus_scope_ancestor(scope->get_parent()))
        {
            scope->set_last_focused_descendant(_focused_widget);
        }

        return true;
    }

    void ui_module_t::clear_focus() noexcept
    {
        if (!_focused_widget)
            return;

        _focused_widget->notify_focus_lost();
        _focused_widget = nullptr;
    }

    bool ui_module_t::dispatch_navigation_action(const ui_navigation_action_t action) noexcept
    {
        const bool handled{ handle_navigation_action(action) };
        const ui_input_ownership_mode_t mode{ resolve_effective_input_ownership_mode() };

        switch (mode)
        {
            case ui_input_ownership_mode_t::passthrough:
                return false;
            case ui_input_ownership_mode_t::ui_priority:
                return handled;
            case ui_input_ownership_mode_t::ui_exclusive:
                return true;
            default:
                return handled;
        }
    }

    bool ui_module_t::dispatch_navigation_action(const std::string_view action_name) noexcept
    {
        ui_navigation_action_t ignored_action{};
        const bool recognized{ try_parse_navigation_action(action_name, ignored_action) };
        const bool handled{ handle_navigation_action(action_name) };
        const ui_input_ownership_mode_t mode{ resolve_effective_input_ownership_mode() };

        switch (mode)
        {
            case ui_input_ownership_mode_t::passthrough:
                return false;
            case ui_input_ownership_mode_t::ui_priority:
                return handled;
            case ui_input_ownership_mode_t::ui_exclusive:
                return recognized || handled;
            default:
                return handled;
        }
    }

    ui_input_ownership_mode_t ui_module_t::get_effective_input_ownership_mode() const noexcept
    {
        return resolve_effective_input_ownership_mode();
    }

    bool ui_module_t::focus_first() noexcept
    {
        const std::vector<ui_widget_t*> focusable{ gather_focusable_widgets() };
        if (focusable.empty())
            return false;

        return set_focus(focusable.front());
    }

    bool ui_module_t::focus_next() noexcept
    {
        const std::vector<ui_widget_t*> focusable{ gather_focusable_widgets() };
        if (focusable.empty())
            return false;

        if (!_focused_widget)
            return set_focus(focusable.front());

        const auto it{
            std::find(focusable.begin(), focusable.end(), _focused_widget)
        };

        if (it == focusable.end())
            return set_focus(focusable.front());

        const auto next_it{ std::next(it) };
        if (next_it == focusable.end())
            return _focus_looping_enabled ? set_focus(focusable.front()) : false;

        return set_focus(*next_it);
    }

    bool ui_module_t::focus_previous() noexcept
    {
        const std::vector<ui_widget_t*> focusable{ gather_focusable_widgets() };
        if (focusable.empty())
            return false;

        if (!_focused_widget)
            return set_focus(focusable.front());

        const auto it{
            std::find(focusable.begin(), focusable.end(), _focused_widget)
        };

        if (it == focusable.end())
            return set_focus(focusable.front());

        if (it == focusable.begin())
        {
            if (_focus_looping_enabled)
                return set_focus(focusable.back());
            return false;
        }

        return set_focus(*std::prev(it));
    }

    bool ui_module_t::navigate(const ui_navigation_direction_t direction) noexcept
    {
        if (!_focused_widget)
            return focus_first();

        if (ui_widget_t* explicit_target{ _focused_widget->get_navigation_target(direction) })
        {
            if (is_widget_in_tree(explicit_target) && is_focusable_widget(explicit_target))
                return set_focus(explicit_target);
        }

        switch (direction)
        {
            case ui_navigation_direction_t::up:
            case ui_navigation_direction_t::left:
                return focus_previous();
            case ui_navigation_direction_t::down:
            case ui_navigation_direction_t::right:
                return focus_next();
            default:
                return false;
        }
    }

    bool ui_module_t::handle_navigation_action(const ui_navigation_action_t action) noexcept
    {
        bool handled{ false };

        switch (action)
        {
            case ui_navigation_action_t::move_up:
                handled = navigate(ui_navigation_direction_t::up);
                if (handled) emit_feedback_event(ui_feedback_event_t::focus_move);
                break;
            case ui_navigation_action_t::move_down:
                handled = navigate(ui_navigation_direction_t::down);
                if (handled) emit_feedback_event(ui_feedback_event_t::focus_move);
                break;
            case ui_navigation_action_t::move_left:
                handled = navigate(ui_navigation_direction_t::left);
                if (handled) emit_feedback_event(ui_feedback_event_t::focus_move);
                break;
            case ui_navigation_action_t::move_right:
                handled = navigate(ui_navigation_direction_t::right);
                if (handled) emit_feedback_event(ui_feedback_event_t::focus_move);
                break;
            case ui_navigation_action_t::focus_next:
                handled = focus_next();
                if (handled) emit_feedback_event(ui_feedback_event_t::focus_move);
                break;
            case ui_navigation_action_t::focus_previous:
                handled = focus_previous();
                if (handled) emit_feedback_event(ui_feedback_event_t::focus_move);
                break;
            case ui_navigation_action_t::accept:
                handled = _focused_widget ? _focused_widget->dispatch_ui_accept() : false;
                if (handled) emit_feedback_event(ui_feedback_event_t::accept);
                break;
            case ui_navigation_action_t::cancel:
                handled = _focused_widget ? _focused_widget->dispatch_ui_cancel() : false;
                if (handled) emit_feedback_event(ui_feedback_event_t::cancel);
                break;
            default:
                handled = false;
                break;
        }

        std::string event_text{ navigation_action_to_string(action) };
        event_text += handled ? " [handled]" : " [ignored]";
        push_debug_navigation_event(std::move(event_text));
        return handled;
    }

    bool ui_module_t::handle_navigation_action(const std::string_view action_name) noexcept
    {
        if (action_name == "ui_up") return handle_navigation_action(ui_navigation_action_t::move_up);
        if (action_name == "ui_down") return handle_navigation_action(ui_navigation_action_t::move_down);
        if (action_name == "ui_left") return handle_navigation_action(ui_navigation_action_t::move_left);
        if (action_name == "ui_right") return handle_navigation_action(ui_navigation_action_t::move_right);
        if (action_name == "ui_next") return handle_navigation_action(ui_navigation_action_t::focus_next);
        if (action_name == "ui_previous" || action_name == "ui_prev")
            return handle_navigation_action(ui_navigation_action_t::focus_previous);
        if (action_name == "ui_accept") return handle_navigation_action(ui_navigation_action_t::accept);
        if (action_name == "ui_cancel") return handle_navigation_action(ui_navigation_action_t::cancel);

        std::string event_text{ "unknown:" };
        event_text.append(action_name.begin(), action_name.end());
        event_text += " [ignored]";
        push_debug_navigation_event(std::move(event_text));
        return false;
    }

    bool ui_module_t::is_widget_in_tree(const ui_widget_t* widget) const noexcept
    {
        if (!widget || !_root)
            return false;

        if (widget == _root.get())
            return true;

        std::vector<const ui_widget_t*> stack;
        stack.push_back(_root.get());

        while (!stack.empty())
        {
            const ui_widget_t* current{ stack.back() };
            stack.pop_back();

            for (const auto& child : current->get_children())
            {
                const ui_widget_t* candidate{ child.get() };
                if (candidate == widget)
                    return true;
                stack.push_back(candidate);
            }
        }

        return false;
    }

    bool ui_module_t::is_focusable_widget(const ui_widget_t* widget) const noexcept
    {
        return widget && widget->can_receive_focus() && is_widget_visible_in_hierarchy(widget);
    }

    bool ui_module_t::is_widget_visible_in_hierarchy(const ui_widget_t* widget) const noexcept
    {
        if (!widget)
            return false;

        for (const ui_widget_t* current{ widget }; current; current = current->get_parent())
        {
            if (current->is_collapsed())
                return false;
        }

        return true;
    }

    bool ui_module_t::is_widget_descendant_of(const ui_widget_t* widget, const ui_widget_t* ancestor) const noexcept
    {
        if (!widget || !ancestor)
            return false;

        for (const ui_widget_t* current{ widget->get_parent() }; current; current = current->get_parent())
        {
            if (current == ancestor)
                return true;
        }

        return false;
    }

    ui_focus_scope_t* ui_module_t::find_nearest_focus_scope_ancestor(ui_widget_t* widget) const noexcept
    {
        return const_cast<ui_focus_scope_t*>(find_nearest_focus_scope_ancestor(static_cast<const ui_widget_t*>(widget)));
    }

    const ui_focus_scope_t* ui_module_t::find_nearest_focus_scope_ancestor(const ui_widget_t* widget) const noexcept
    {
        if (!widget)
            return nullptr;

        for (const ui_widget_t* current{ widget }; current; current = current->get_parent())
        {
            if (const auto* scope{ dynamic_cast<const ui_focus_scope_t*>(current) })
                return scope;
        }

        return nullptr;
    }

    std::vector<ui_focus_scope_t*> ui_module_t::gather_visible_focus_scopes() const
    {
        std::vector<ui_focus_scope_t*> scopes;
        if (!_root)
            return scopes;

        std::vector<ui_widget_t*> stack;
        stack.push_back(_root.get());

        while (!stack.empty())
        {
            ui_widget_t* current{ stack.back() };
            stack.pop_back();

            if (current->is_collapsed())
                continue;

            if (auto* scope{ dynamic_cast<ui_focus_scope_t*>(current) })
                scopes.push_back(scope);

            for (const auto& child : current->get_children())
                stack.push_back(child.get());
        }

        return scopes;
    }

    ui_widget_t* ui_module_t::resolve_focus_target_for_scope(ui_focus_scope_t& scope) noexcept
    {
        std::vector<ui_widget_t*> focusable;
        gather_focusable_widgets_in_subtree(scope, focusable);
        if (focusable.empty())
            return nullptr;

        switch (scope.get_focus_on_show_policy())
        {
            case ui_focus_on_show_policy_t::first:
                return focusable.front();
            case ui_focus_on_show_policy_t::last:
            {
                if (ui_widget_t* last{ scope.get_last_focused_descendant() })
                {
                    const bool valid{
                        std::find(focusable.begin(), focusable.end(), last) != focusable.end()
                    };
                    if (valid)
                        return last;
                }
                return focusable.front();
            }
            case ui_focus_on_show_policy_t::explicit_target:
            {
                if (ui_widget_t* explicit_target{ scope.get_explicit_focus_target() })
                {
                    const bool valid{
                        std::find(focusable.begin(), focusable.end(), explicit_target) != focusable.end()
                    };
                    if (valid)
                        return explicit_target;
                }
                return focusable.front();
            }
            default:
                return focusable.front();
        }
    }

    bool ui_module_t::activate_focus_scope(ui_focus_scope_t& scope) noexcept
    {
        focus_scope_runtime_t& runtime{ _focus_scope_runtime[&scope] };
        runtime.was_visible = true;

        if (_active_focus_scope == &scope)
            return true;

        if (_focused_widget && !is_widget_descendant_of(_focused_widget, &scope))
            runtime.restore_focus_target = _focused_widget;

        _active_focus_scope = &scope;

        if (ui_widget_t* target{ resolve_focus_target_for_scope(scope) })
            return set_focus(target);

        return false;
    }

    void ui_module_t::deactivate_focus_scope(ui_focus_scope_t& scope) noexcept
    {
        auto runtime_it{ _focus_scope_runtime.find(&scope) };
        if (runtime_it != _focus_scope_runtime.end())
            runtime_it->second.was_visible = false;

        if (_active_focus_scope != &scope)
            return;

        ui_widget_t* restore_target{ nullptr };
        if (runtime_it != _focus_scope_runtime.end())
            restore_target = runtime_it->second.restore_focus_target;

        _active_focus_scope = nullptr;
        if (restore_target && is_widget_in_tree(restore_target) && is_focusable_widget(restore_target))
            (void)set_focus(restore_target);
        else if (_focused_widget && is_widget_descendant_of(_focused_widget, &scope))
            clear_focus();
    }

    void ui_module_t::reconcile_focus_scopes() noexcept
    {
        const std::vector<ui_focus_scope_t*> visible_scopes{ gather_visible_focus_scopes() };
        std::unordered_set<ui_focus_scope_t*> current_visible_set;
        current_visible_set.reserve(visible_scopes.size());
        for (ui_focus_scope_t* scope : visible_scopes)
            current_visible_set.insert(scope);

        for (ui_focus_scope_t* scope : _visible_focus_scopes)
        {
            if (!current_visible_set.contains(scope))
                deactivate_focus_scope(*scope);
        }

        for (ui_focus_scope_t* scope : visible_scopes)
        {
            if (!_visible_focus_scopes.contains(scope) && scope->is_focus_trap_enabled())
                (void)activate_focus_scope(*scope);
        }

        _visible_focus_scopes = std::move(current_visible_set);
    }

    void ui_module_t::gather_focusable_widgets(const ui_widget_t& widget, std::vector<ui_widget_t*>& out) const
    {
        for (const auto& child : widget.get_children())
        {
            ui_widget_t* raw{ child.get() };
            if (is_focusable_widget(raw))
                out.push_back(raw);

            gather_focusable_widgets(*raw, out);
        }
    }

    void ui_module_t::gather_focusable_widgets_in_subtree(const ui_widget_t& widget, std::vector<ui_widget_t*>& out) const
    {
        gather_focusable_widgets(widget, out);
    }

    std::vector<ui_widget_t*> ui_module_t::gather_focusable_widgets() const
    {
        std::vector<ui_widget_t*> result;
        if (!_root)
            return result;

        if (_active_focus_scope && is_widget_in_tree(_active_focus_scope) && _active_focus_scope->is_focus_trap_enabled())
            gather_focusable_widgets_in_subtree(*_active_focus_scope, result);
        else
            gather_focusable_widgets(*_root, result);

        return result;
    }

    ui_input_ownership_mode_t ui_module_t::resolve_effective_input_ownership_mode() const noexcept
    {
        if (_runtime_input_ownership_override.has_value())
            return *_runtime_input_ownership_override;

        if (_active_focus_scope && is_widget_in_tree(_active_focus_scope))
        {
            if (_active_focus_scope->has_input_ownership_override())
                return _active_focus_scope->get_input_ownership_override();

            if (_active_focus_scope->is_focus_trap_enabled())
                return ui_input_ownership_mode_t::ui_exclusive;
        }

        return _input_ownership_mode;
    }

    const char* ui_module_t::input_ownership_mode_to_string(const ui_input_ownership_mode_t mode) noexcept
    {
        switch (mode)
        {
            case ui_input_ownership_mode_t::passthrough: return "passthrough";
            case ui_input_ownership_mode_t::ui_priority: return "ui_priority";
            case ui_input_ownership_mode_t::ui_exclusive: return "ui_exclusive";
            default: return "unknown";
        }
    }

    bool ui_module_t::try_parse_navigation_action(const std::string_view action_name,
                                                  ui_navigation_action_t& out_action) const noexcept
    {
        if (action_name == "ui_up")
        {
            out_action = ui_navigation_action_t::move_up;
            return true;
        }
        if (action_name == "ui_down")
        {
            out_action = ui_navigation_action_t::move_down;
            return true;
        }
        if (action_name == "ui_left")
        {
            out_action = ui_navigation_action_t::move_left;
            return true;
        }
        if (action_name == "ui_right")
        {
            out_action = ui_navigation_action_t::move_right;
            return true;
        }
        if (action_name == "ui_next")
        {
            out_action = ui_navigation_action_t::focus_next;
            return true;
        }
        if (action_name == "ui_previous" || action_name == "ui_prev")
        {
            out_action = ui_navigation_action_t::focus_previous;
            return true;
        }
        if (action_name == "ui_accept")
        {
            out_action = ui_navigation_action_t::accept;
            return true;
        }
        if (action_name == "ui_cancel")
        {
            out_action = ui_navigation_action_t::cancel;
            return true;
        }

        return false;
    }

    std::string ui_module_t::navigation_action_to_string(const ui_navigation_action_t action)
    {
        switch (action)
        {
            case ui_navigation_action_t::move_up: return "move_up";
            case ui_navigation_action_t::move_down: return "move_down";
            case ui_navigation_action_t::move_left: return "move_left";
            case ui_navigation_action_t::move_right: return "move_right";
            case ui_navigation_action_t::focus_next: return "focus_next";
            case ui_navigation_action_t::focus_previous: return "focus_previous";
            case ui_navigation_action_t::accept: return "accept";
            case ui_navigation_action_t::cancel: return "cancel";
            default: return "unknown";
        }
    }

    void ui_module_t::push_debug_navigation_event(std::string event_text)
    {
        if (event_text.empty())
            return;

        if (_debug_navigation_events.size() >= k_max_debug_navigation_events)
            _debug_navigation_events.erase(_debug_navigation_events.begin());

        _debug_navigation_events.emplace_back(std::move(event_text));
    }

    void ui_module_t::emit_feedback_event(const ui_feedback_event_t event) const noexcept
    {
        if (_feedback_callback)
            _feedback_callback(event);
    }
} // namespace carrot::ui
