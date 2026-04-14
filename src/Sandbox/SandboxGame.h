//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

#include <string>

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
        void configure_input_routing() noexcept;
        void bootstrap_runtime_ui() noexcept;
        void begin_interact_rebind();
        void finish_pending_input_rebind();
        void restore_default_input_bindings();
        void set_rebind_status(std::string message, float seconds = 4.f) noexcept;

        carrot::input::gameplay_input_router_t _input;
        carrot::input::input_binding_store_t _input_binding_store;
        carrot::input::input_rebind_session_t _input_rebind_session;
        std::string _rebind_status_message;
        float _rebind_status_seconds{ 0.f };
    };
} // namespace sandbox
