//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>
#include <memory>

namespace sandbox {
    class sandbox_game_t;

    class sandbox_t final : public carrot::core::ce_application_t
    {
    public:
        sandbox_t();
        ~sandbox_t() override;

        void describe_boot_prewarm(carrot::core::boot_prewarm_plan_t& plan) const override;
        void start(carrot::core::game_context_t& game) override;

        void on_tick(float delta_time) override;
        void on_render_overlay() override;
        void on_window_focus_changed(const carrot::events::window_focused_t& e) override;
        void on_key(const carrot::events::key_event_t& e) override;
        void on_mouse_moved(const carrot::events::mouse_moved_event_t& e) override;
        void on_mouse_button(const carrot::events::mouse_button_event_t& e) override;
        void on_mouse_scrolled(const carrot::events::mouse_scrolled_event_t& e) override;

    private:
        carrot::core::game_context_t* _game{ nullptr };
        std::unique_ptr<sandbox_game_t> _runtime;
    };
} // namespace sandbox
