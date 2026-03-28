//
// Created by Zack Shrout on 3/28/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Sampler.h"
#include "MetalCommon.h"

namespace carrot::rhi::metal {
    class metal_sampler_t final : public rhi_sampler_t
    {
    public:
        metal_sampler_t(MTL::SamplerState* sampler, const sampler_desc_t& desc)
            : rhi_sampler_t{ desc }, _sampler{ sampler } {}

        ~metal_sampler_t() override
        {
            if (_sampler)
            {
                _sampler->release();
                _sampler = nullptr;
            }
        }

        [[nodiscard]] MTL::SamplerState* mtl_sampler() const noexcept { return _sampler; }

    private:
        MTL::SamplerState* _sampler{ nullptr };
    };
} // namespace carrot::rhi::metal
