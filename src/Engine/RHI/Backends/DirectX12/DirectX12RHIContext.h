//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/RHI.h"

namespace carrot::rhi::dx12 {
    class direct_x12_rhi_context_t final : public rhi_context_t
    {
    public:
        explicit direct_x12_rhi_context_t(const rhi_desc_t& desc);
        ~direct_x12_rhi_context_t() override;

        void begin_frame() override;
        void record_frame() override;
        void end_frame() override;

        void resize(uint32_t width, uint32_t height) override;

        [[nodiscard]] rhi_device_t* get_device() const noexcept override;
        [[nodiscard]] rhi_swapchain_t* get_swapchain() const noexcept override;
        [[nodiscard]] rhi_command_queue_t* get_command_queue() const noexcept override;

        void wait_idle() override;

    private:
    };
} // namespace carrot::rhi::dx12