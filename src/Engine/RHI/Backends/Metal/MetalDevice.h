//
// Created by Zack Shrout on 2/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "MetalCommon.h"
#include "RHI/CommandQueue.h"
#include "RHI/Device.h"

namespace carrot::rhi::metal {
    class metal_device_t final : public rhi_device_t
    {
    public:
        explicit metal_device_t(MTL::Device* device);
        ~metal_device_t() override;

        [[nodiscard]] rhi_command_queue_t* create_command_queue(queue_type type);

        // Accessors for internal use
        [[nodiscard]] MTL::Device* mtl_device() const noexcept { return _device; }

    private:
        MTL::Device* _device{ nullptr };
    };
} // namespace carrot::rhi::metal
