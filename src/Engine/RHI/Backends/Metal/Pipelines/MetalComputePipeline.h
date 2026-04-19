//
// Created by Zack Shrout on 4/18/2026.
//

#pragma once

#include "RHI/Backends/Metal/MetalCommon.h"
#include "RHI/Pipeline.h"

#include <memory>

namespace carrot::assets {
    class shader_file_provider_t;
}

namespace carrot::rhi::metal {
    class metal_device_t;

    class metal_compute_pipeline_t final : public rhi_compute_pipeline_t
    {
    public:
        metal_compute_pipeline_t(const metal_device_t& device,
                                 const assets::shader_file_provider_t& shader_files,
                                 const compute_pipeline_create_info_t& info);

        ~metal_compute_pipeline_t() override = default;

        [[nodiscard]] bool is_valid() const noexcept { return _state != nullptr; }
        [[nodiscard]] MTL::ComputePipelineState* state() const noexcept { return _state.get(); }

    private:
        [[nodiscard]] static MTL::Library* load_library(MTL::Device* device,
                                                        const assets::shader_file_provider_t& shader_files,
                                                        std::string_view virtual_path);
        [[nodiscard]] static MTL::Function* load_function(MTL::Library* library);

        std::shared_ptr<MTL::ComputePipelineState> _state;
    };
} // namespace carrot::rhi::metal
