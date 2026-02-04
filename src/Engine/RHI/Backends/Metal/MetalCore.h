//
// Created by Zack Shrout on 2/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "MetalCommon.h"
#include <memory>

namespace carrot::rhi::metal {
    constexpr uint32_t k_max_frames_in_flight{ 3 };

    // ─────────────────────────────────────────────────────────────────────────────────────────────────
    // Note: the next several structs are RAII wrappers designed to destroy their resource on scope exit
    // ─────────────────────────────────────────────────────────────────────────────────────────────────

    struct render_pipeline_state_t
    {
        MTL::RenderPipelineState* state{ nullptr };

        render_pipeline_state_t() = default;
        explicit render_pipeline_state_t(MTL::RenderPipelineState* s) : state(s) {}
        ~render_pipeline_state_t() { if (state) state->release(); }

        DISABLE_COPY(render_pipeline_state_t)

        render_pipeline_state_t(render_pipeline_state_t&& other) noexcept
            : state(other.state) { other.state = nullptr; }

        render_pipeline_state_t& operator=(render_pipeline_state_t&& other) noexcept
        {
            if (this != &other)
            {
                if (state) state->release();
                state = other.state;
                other.state = nullptr;
            }
            return *this;
        }

        MTL::RenderPipelineState* operator->() const { return state; }
        explicit operator bool() const { return state != nullptr; }
    };
} // namespace carrot::rhi::metal
