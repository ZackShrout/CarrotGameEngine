//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Sandbox.h"

#include "SandboxGame.h"

namespace sandbox {
    namespace {
        constexpr std::string_view k_initial_scene_id{ "scene.sandbox.town" };
    }

    sandbox_t::sandbox_t() = default;

    sandbox_t::~sandbox_t() = default;

    void sandbox_t::describe_boot_prewarm(carrot::core::boot_prewarm_plan_t& plan) const
    {
        plan.scene_ids.emplace_back(k_initial_scene_id);
        plan.font_ids.emplace_back("font.engine.roboto_regular");
    }

    void sandbox_t::start(carrot::core::game_context_t& game)
    {
        _game = &game;
        _runtime = std::make_unique<sandbox_game_t>(game);
        _runtime->start();
    }

    void sandbox_t::on_tick(const float delta_time)
    {
        if (_runtime)
            _runtime->tick(delta_time);
    }

    void sandbox_t::on_render_overlay()
    {
        if (_runtime)
            _runtime->render_overlay();
    }

    void sandbox_t::on_window_focus_changed(const carrot::events::window_focused_t& e)
    {
        ce_application_t::on_window_focus_changed(e);
        if (_runtime)
            _runtime->on_window_focus_changed(e);
    }

    void sandbox_t::on_key(const carrot::events::key_event_t& e)
    {
        ce_application_t::on_key(e);
        if (_runtime)
            _runtime->on_key(e);
    }

    void sandbox_t::on_mouse_moved(const carrot::events::mouse_moved_event_t& e)
    {
        ce_application_t::on_mouse_moved(e);
        if (_runtime)
            _runtime->on_mouse_moved(e);
    }

    void sandbox_t::on_mouse_button(const carrot::events::mouse_button_event_t& e)
    {
        ce_application_t::on_mouse_button(e);
        if (_runtime)
            _runtime->on_mouse_button(e);
    }

    void sandbox_t::on_mouse_scrolled(const carrot::events::mouse_scrolled_event_t& e)
    {
        ce_application_t::on_mouse_scrolled(e);
        if (_runtime)
            _runtime->on_mouse_scrolled(e);
    }
} // namespace sandbox
