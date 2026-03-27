//
// Created by Zack Shrout on 2/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "MetalCommandQueue.h"

namespace carrot::rhi::metal {
    metal_command_queue_t::metal_command_queue_t(MTL::CommandQueue* queue) : _queue{ queue }
    {
    }

    metal_command_queue_t::~metal_command_queue_t()
    {
        if (_queue) _queue->release();
    }

    void metal_command_queue_t::submit(rhi_command_list_t* cmd_list, rhi_fence_t* fence_to_signal,
        rhi_semaphore_t* wait_semaphore, rhi_semaphore_t* signal_semaphore)
    {
        // TODO: real implementation when we  have command lists. For now, just log.
        LOG_GRAPHICS_TRACE("Metal submit called (stub)");
    }

    void metal_command_queue_t::wait_idle()
    {
        // NOTE: Metal queues don't have direct wait_idle; can use a dummy fence
        //       or just sync. No-op for now.
    }
} // namespace carrot::rhi::metal
