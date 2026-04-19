//
// Created by Zack Shrout on 4/18/2026.
//

#pragma once

#include "RHI/Backends/DirectX12/DirectX12Common.h"
#include "RHI/Pipeline.h"

namespace carrot::assets {
    class shader_file_provider_t;
}

namespace carrot::rhi::dx12 {
    class dx12_compute_pipeline_t final : public rhi_compute_pipeline_t
    {
    public:
        dx12_compute_pipeline_t(ID3D12Device* device,
                                assets::shader_file_provider_t& shader_files,
                                const compute_pipeline_create_info_t& info);
        ~dx12_compute_pipeline_t() override;

        [[nodiscard]] bool is_valid() const noexcept
        {
            return _root_signature != nullptr && _pipeline_state != nullptr;
        }

        [[nodiscard]] ID3D12RootSignature* root_signature() const noexcept { return _root_signature; }
        [[nodiscard]] ID3D12PipelineState* pipeline_state() const noexcept { return _pipeline_state; }
    private:
        ID3D12Device* _device{ nullptr };
        ID3D12RootSignature* _root_signature{ nullptr };
        ID3D12PipelineState* _pipeline_state{ nullptr };
    };
} // namespace carrot::rhi::dx12
