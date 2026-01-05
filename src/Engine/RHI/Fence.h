//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::rhi {
    class fence_t
    {
    public:
        virtual ~fence_t() = default;
        virtual void wait(uint64_t timeout_ns = ~0ULL) = 0;
        virtual void reset() = 0;
    };
} // namespace carrot::rhi