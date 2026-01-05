//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::rhi {
    class command_list_t;
    class fence_t;
    class semaphore_t;

    enum class queue_type : std::uint8_t { graphics, compute, transfer };

    class command_queue_t
    {
        virtual ~command_queue_t() = default;

        virtual void submit(command_list_t* cmd_list, fence_t* fence_to_signal = nullptr,
                            semaphore_t* wait_semaphore = nullptr, semaphore_t* signal_semaphore = nullptr) = 0;

        virtual void wait_idle() = 0;
    };
} // namespace carrot::rhi