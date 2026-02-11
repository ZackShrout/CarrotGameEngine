//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/AudioModule.h"
#include "Common/CommonHeaders.h"
#include "Renderer/Renderer.h"
#include "Utils/MulticastDelegate.h"

namespace carrot {
    namespace rhi {
        class rhi_context_t;
    }

    namespace core {
        class ce_application_t;
    }

    DECLARE_MULTICAST_DELEGATE(on_tick_t, float/* dt*/);

    class engine_t
    {
    public:
        engine_t() noexcept;
        ~engine_t();

        DISABLE_COPY_AND_MOVE(engine_t)

        void run(core::ce_application_t* app);
        [[nodiscard]] static engine_t& get() noexcept;

        void request_quit() noexcept { _should_quit = true; }
        [[nodiscard]] bool should_quit() const noexcept { return _should_quit; }
        [[nodiscard]] float get_delta_time() const noexcept { return _delta_time; }
        [[nodiscard]] uint32_t get_fps() const noexcept { return _current_fps; }

    private:
        void tick();

        bool                                    _should_quit{ false };
        float                                   _delta_time{ 0.f };
        uint32_t                                _current_fps{ 0 };

        std::unique_ptr<renderer::renderer_t>   _renderer{ nullptr };
        std::unique_ptr<audio::audio_module_t>  _audio_module{ nullptr };

        on_tick_t                               _on_tick;
    };
} // namespace carrot
