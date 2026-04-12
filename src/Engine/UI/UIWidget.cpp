//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "UIWidget.h"

#include "Renderer/Renderer.h"

#include <atomic>
#include <cmath>
#include <iterator>

namespace carrot::ui {
    namespace {
        std::atomic<ui_widget_id_t> g_next_widget_id{ 1 };

        [[nodiscard]] bool nearly_equal(const float lhs, const float rhs) noexcept
        {
            return std::fabs(lhs - rhs) <= 0.0001f;
        }

        [[nodiscard]] bool rect_equals(const ui_rect_t& lhs, const ui_rect_t& rhs) noexcept
        {
            return nearly_equal(lhs.x, rhs.x) &&
                   nearly_equal(lhs.y, rhs.y) &&
                   nearly_equal(lhs.width, rhs.width) &&
                   nearly_equal(lhs.height, rhs.height);
        }
    } // anonymous namespace

    // PUBLIC

    ui_widget_t::ui_widget_t() : _id { generate_next_id() } {}

    const std::vector<std::unique_ptr<ui_widget_t>>& ui_widget_t::get_children() const noexcept
    {
        return _children;
    }

    bool ui_widget_t::is_enabled() const noexcept
    {
        return has_flag(ui_widget_flag_bits_t::enabled);
    }

    bool ui_widget_t::is_focusable() const noexcept
    {
        return has_flag(ui_widget_flag_bits_t::focusable);
    }

    bool ui_widget_t::is_visible() const noexcept
    {
        return _visibility == ui_widget_visibility_t::visible;
    }

    bool ui_widget_t::is_hidden() const noexcept
    {
        return _visibility == ui_widget_visibility_t::hidden;
    }

    bool ui_widget_t::is_collapsed() const noexcept
    {
        return _visibility == ui_widget_visibility_t::collapsed;
    }

    bool ui_widget_t::can_receive_focus() const noexcept
    {
        return is_enabled() && is_focusable() && !is_collapsed();
    }

    ui_widget_t* ui_widget_t::get_navigation_target(const ui_navigation_direction_t direction) noexcept
    {
        const size_t index{ static_cast<size_t>(direction) };
        if (index >= std::size(_navigation_targets))
            return nullptr;

        return _navigation_targets[index];
    }

    const ui_widget_t* ui_widget_t::get_navigation_target(const ui_navigation_direction_t direction) const noexcept
    {
        const size_t index{ static_cast<size_t>(direction) };
        if (index >= std::size(_navigation_targets))
            return nullptr;

        return _navigation_targets[index];
    }

    void ui_widget_t::set_enabled(const bool enabled) noexcept
    {
        set_flag(ui_widget_flag_bits_t::enabled, enabled);
    }

    void ui_widget_t::set_focusable(const bool focusable) noexcept
    {
        set_flag(ui_widget_flag_bits_t::focusable, focusable);
    }

    void ui_widget_t::set_visibility(const ui_widget_visibility_t visibility) noexcept
    {
        if (_visibility == visibility)
            return;

        _visibility = visibility;
        invalidate_layout();
    }

    void ui_widget_t::set_desired_size(const ui_size_t size) noexcept
    {
        if (size.width < 0.f || size.height < 0.f)
            return;

        if (_desired_size.width == size.width && _desired_size.height == size.height)
            return;

        _desired_size = size;
        invalidate_layout();
    }

    void ui_widget_t::set_min_size(const ui_size_t size) noexcept
    {
        const ui_size_t clamped{
            .width = std::max(0.f, size.width),
            .height = std::max(0.f, size.height)
        };

        if (_min_size.width == clamped.width && _min_size.height == clamped.height)
            return;

        _min_size = clamped;
        _max_size.width = std::max(_max_size.width, _min_size.width);
        _max_size.height = std::max(_max_size.height, _min_size.height);
        invalidate_layout();
    }

    void ui_widget_t::set_max_size(const ui_size_t size) noexcept
    {
        const ui_size_t clamped{
            .width = std::max(0.f, size.width),
            .height = std::max(0.f, size.height)
        };

        if (_max_size.width == clamped.width && _max_size.height == clamped.height)
            return;

        _max_size = clamped;
        _min_size.width = std::min(_min_size.width, _max_size.width);
        _min_size.height = std::min(_min_size.height, _max_size.height);
        invalidate_layout();
    }

    void ui_widget_t::set_main_axis_size_rule(const ui_main_axis_size_rule_t rule) noexcept
    {
        if (_main_axis_size_rule == rule)
            return;

        _main_axis_size_rule = rule;
        invalidate_layout();
    }

    void ui_widget_t::set_flex_weight(const float weight) noexcept
    {
        const float clamped_weight{ std::max(0.f, weight) };
        if (_flex_weight == clamped_weight)
            return;

        _flex_weight = clamped_weight;
        invalidate_layout();
    }

    void ui_widget_t::set_navigation_target(const ui_navigation_direction_t direction, ui_widget_t* target) noexcept
    {
        const size_t index{ static_cast<size_t>(direction) };
        if (index >= std::size(_navigation_targets))
            return;

        _navigation_targets[index] = target;
    }

    void ui_widget_t::clear_navigation_target(const ui_navigation_direction_t direction) noexcept
    {
        set_navigation_target(direction, nullptr);
    }

    void ui_widget_t::invalidate_layout() noexcept
    {
        if (_layout_dirty)
        {
            if (_parent && !_parent->_layout_dirty)
                _parent->invalidate_layout();
            return;
        }

        _layout_dirty = true;

        if (_parent)
            _parent->invalidate_layout();
    }

    void ui_widget_t::add_child(std::unique_ptr<ui_widget_t> child) noexcept
    {
        if (!child) return;

        CE_ASSERT(child.get() != this, "A widget cannot be added as its own child.");
        CE_ASSERT(!child->get_parent(), "Cannot add a child that already has a parent.");

        ui_widget_t* raw_child{ child.get() };
        raw_child->_parent = this;

        _children.push_back(std::move(child));

        // NOTE: Must use raw pointer from this point: 'child' is moved-from
        if (_is_attached)
            raw_child->attach_subtree();

        invalidate_layout();
        on_child_added(*raw_child);
    }

    std::unique_ptr<ui_widget_t> ui_widget_t::remove_child(ui_widget_t& child) noexcept
    {
        const auto it{ std::find_if(_children.begin(), _children.end(),
            [&child](const std::unique_ptr<ui_widget_t>& candidate)
            {
                return candidate.get() == &child;
            }) };

        if (it == _children.end()) return nullptr;

        ui_widget_t* raw_child{ it->get() };

        if (raw_child->_is_attached)
            raw_child->detach_subtree();

        raw_child->_parent = nullptr;

        std::unique_ptr<ui_widget_t> removed{ std::move(*it) };
        _children.erase(it);

        invalidate_layout();
        on_child_removed(*removed);

        return removed;
    }

    void ui_widget_t::remove_all_children() noexcept
    {
        for (auto& child : _children)
        {
            if (child->_is_attached)
                child->detach_subtree();

            child->_parent = nullptr;
            on_child_removed(*child);
        }

        _children.clear();
        invalidate_layout();
    }

    void ui_widget_t::tick_tree(float delta_time) noexcept
    {
        on_tick(delta_time);

        for (const auto& child : _children)
            child->tick_tree(delta_time);
    }

    void ui_widget_t::layout_tree(const ui_rect_t& bounds) noexcept
    {
        layout_subtree(bounds, false);
    }

    void ui_widget_t::render_tree(renderer::renderer_t& renderer) const noexcept
    {
        if (!is_visible())
            return;

        on_render(renderer);

        for (const auto& child : _children)
        {
            if (child)
                child->render_tree(renderer);
        }
    }

    void ui_widget_t::attach_to_tree() noexcept
    {
        attach_subtree();
    }

    void ui_widget_t::detach_from_tree() noexcept
    {
        detach_subtree();
    }

    bool ui_widget_t::get_debug_visual_style(ui_debug_visual_style_t& out) const noexcept
    {
        if (!can_receive_focus())
            return false;

        out = ui_debug_visual_style_t{};
        return true;
    }

    // PRIVATE

    bool ui_widget_t::has_flag(ui_widget_flag_bits_t flag) const noexcept
    {
        return (_flags & static_cast<ui_widget_flags_t>(flag)) != 0u;
    }

    void ui_widget_t::set_flag(ui_widget_flag_bits_t flag, bool enabled) noexcept
    {
        const ui_widget_flags_t mask{ static_cast<ui_widget_flags_t>(flag) };

        if (enabled)
            _flags |= mask;
        else
            _flags &= ~mask;
    }

    void ui_widget_t::arrange_children(const ui_rect_t& bounds) noexcept
    {
        for (const auto& child : _children)
        {
            if (child->is_collapsed())
                continue;

            child->layout_tree(bounds);
        }
    }

    void ui_widget_t::layout_subtree(const ui_rect_t& bounds, const bool parent_layout_dirty) noexcept
    {
        const bool bounds_changed{ !rect_equals(_layout_bounds, bounds) };
        const bool needs_layout{ parent_layout_dirty || _layout_dirty || bounds_changed };

        if (!needs_layout)
            return;

        _layout_bounds = bounds;
        _layout_dirty = false;

        on_layout_updated(_layout_bounds);
        arrange_children(_layout_bounds);
    }

    void ui_widget_t::attach_subtree() noexcept
    {
        if (_is_attached) return;

        _is_attached = true;
        on_attached_to_tree();

        for (const auto& child : _children)
            child->attach_subtree();
    }

    void ui_widget_t::detach_subtree() noexcept
    {
        if (!_is_attached) return;

        for (const auto& child : _children)
            child->detach_subtree();

        on_detached_from_tree();
        _is_attached = false;
    }

    ui_widget_id_t ui_widget_t::generate_next_id() noexcept
    {
        return g_next_widget_id.fetch_add(1, std::memory_order_relaxed);
    }
} // namespace carrot::ui
