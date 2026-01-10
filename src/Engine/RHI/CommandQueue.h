//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::rhi {
    class rhi_command_list_t;
    class rhi_fence_t;
    class rhi_semaphore_t;

    enum class queue_type : std::uint8_t { graphics, compute, transfer };

    class rhi_command_queue_t
    {
    public:
        virtual ~rhi_command_queue_t() = default;

        virtual void submit(rhi_command_list_t* cmd_list, rhi_fence_t* fence_to_signal = nullptr,
                            rhi_semaphore_t* wait_semaphore = nullptr, rhi_semaphore_t* signal_semaphore = nullptr) = 0;

        virtual void wait_idle() = 0;
    };
} // namespace carrot::rhi