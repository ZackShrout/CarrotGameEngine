//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

namespace carrot::rhi {
    class rhi_command_list_t
    {
    public:
        virtual ~rhi_command_list_t() = default;

        virtual void reset() = 0;
        virtual void begin_recording() = 0;
        virtual void end_recording() = 0;

        // More commands will come later: begin_render_pass, bind_pipeline, draw, etc.
    };
} // namespace carrot::rhi