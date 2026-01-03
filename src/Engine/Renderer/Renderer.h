//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#pragma once

namespace carrot::renderer {
    class renderer_t
    {
    public:
        renderer_t() = default;
        virtual ~renderer_t() = default;

        virtual void init() = 0;
        virtual void shutdown() = 0;

        virtual void begin_frame() = 0;
        virtual void render_frame() = 0;
        virtual void end_frame() = 0;

        virtual void reload_pipeline() = 0;
    };

    extern renderer_t* create_backend();
} // namespace carrot::renderer
