//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::core {
    class application_t
    {
    public:
        application_t(const application_t&) = delete;

        application_t& operator=(const application_t&) = delete;

        application_t(application_t&&) = delete;

        application_t& operator=(application_t&&) = delete;

        [[nodiscard]] static application_t& get() noexcept;

        void run();

        void request_quit() noexcept { _should_quit = true; }

        [[nodiscard]] bool should_quit() const noexcept { return _should_quit; }
        [[nodiscard]] float get_delta_time() const noexcept { return _delta_time; }
        [[nodiscard]] uint32_t get_fps() const noexcept { return _current_fps; }

    private:
        application_t() noexcept;

        ~application_t();

        void init();

        void shutdown();

        void tick();

    private:
        bool _should_quit{ false };

        uint64_t _last_tick_time{ 0 };
        float _delta_time{ 0.f };

        uint32_t _frame_counter{ 0 };
        float _fps_timer{ 0.f };
        uint32_t _current_fps{ 0 };

        bool _debug_overlay_initialized{ false };
    };
} // namespace carrot
