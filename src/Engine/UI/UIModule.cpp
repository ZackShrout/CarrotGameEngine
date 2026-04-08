//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "UIModule.h"

namespace carrot::ui {
    // PUBLIC

    void ui_module_t::init()
    {
        if (_is_initialized) return;

        if (!_root)
            _root = std::make_unique<ui_root_widget_t>();

        _root->attach_to_tree();

        _is_initialized = true;
    }

    void ui_module_t::shutdown()
    {
        if (!_is_initialized) return;

        if (_root)
            _root->detach_from_tree();

        _root.reset();

        _is_initialized = false;
    }

    void ui_module_t::update(const float delta_time) noexcept
    {
        if (!_is_initialized || !_root) return;

        _root->tick_tree(delta_time);
    }

    void ui_module_t::set_root(std::unique_ptr<ui_root_widget_t> root) noexcept
    {
        if (!root) return;

        if (_root && _is_initialized)
            _root->detach_from_tree();

        _root = std::move(root);

        if (_is_initialized)
            _root->attach_to_tree();
    }

    void ui_module_t::clear_root() noexcept
    {
        if (_root && _is_initialized)
            _root->detach_from_tree();

        _root = std::make_unique<ui_root_widget_t>();

        if (_is_initialized)
            _root->attach_to_tree();
    }
} // namespace carrot::ui