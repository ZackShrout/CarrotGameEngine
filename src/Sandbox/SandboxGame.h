//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

namespace sandbox {
    class sandbox_game_t final : public carrot::core::game_runtime_t
    {
    public:
        explicit sandbox_game_t(carrot::core::game_context_t& game) noexcept;

        void start() override;
        void tick(float delta_time) override;
        void on_key(const carrot::events::key_event_t& e) override;
        void on_mouse_moved(const carrot::events::mouse_moved_event_t& e) override;
        void on_mouse_button(const carrot::events::mouse_button_event_t& e) override;
        void on_mouse_scrolled(const carrot::events::mouse_scrolled_event_t& e) override;

        [[nodiscard]] carrot::input::gameplay_input_router_t& input() noexcept { return _input; }
        [[nodiscard]] const carrot::input::gameplay_input_router_t& input() const noexcept { return _input; }

    private:
        void configure_fallback_input_actions();
        void configure_default_input_actions();
        void bootstrap_runtime_ui() noexcept;

        carrot::input::gameplay_input_router_t _input;
    };
} // namespace sandbox
