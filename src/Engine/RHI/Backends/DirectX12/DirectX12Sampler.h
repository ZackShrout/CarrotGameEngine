//
// Created by zshro on 3/30/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Sampler.h"

namespace carrot::rhi::dx12 {
    class dx12_sampler_t final : public rhi_sampler_t
    {
    public:
        explicit dx12_sampler_t(const sampler_desc_t& desc) : rhi_sampler_t{ desc } {}
    };
} // namespace carrot::rhi::dx12