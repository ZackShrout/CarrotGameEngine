//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "UIWidget.h"

#include <atomic>

namespace carrot::ui {
    namespace {
        std::atomic<ui_widget_id_t> g_next_widget_id{ 1 };
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
        _visibility = visibility;
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
    }

    void ui_widget_t::tick_tree(float delta_time) noexcept
    {
        on_tick(delta_time);

        for (const auto& child : _children)
            child->tick_tree(delta_time);
    }

    void ui_widget_t::attach_to_tree() noexcept
    {
        attach_subtree();
    }

    void ui_widget_t::detach_from_tree() noexcept
    {
        detach_subtree();
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