//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include "Engine/Common/CommonHeaders.h"

namespace carrot::core {
    class application_t
    {
    public:
        application_t() noexcept;
        ~application_t();

        DISABLE_COPY_AND_MOVE(application_t);

        void run();
        [[nodiscard]] static application_t& get() noexcept;

        void request_quit() noexcept { _should_quit = true; }
        [[nodiscard]] bool should_quit() const noexcept { return _should_quit; }
        [[nodiscard]] float get_delta_time() const noexcept { return _delta_time; }
        [[nodiscard]] uint32_t get_fps() const noexcept { return _current_fps; }

    private:
        void tick();

        bool        _should_quit{ false };
        float       _delta_time{ 0.f };
        uint32_t    _current_fps{ 0 };
    };
} // namespace carrot
