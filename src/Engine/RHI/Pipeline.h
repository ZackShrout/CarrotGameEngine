//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace carrot::rhi {
    class rhi_buffer_t;

    constexpr std::uint32_t k_max_compute_buffer_bindings{ 4u };
    constexpr std::uint32_t k_compute_constant_register{ 7u };

    struct indexed_indirect_draw_command_t
    {
        std::uint32_t index_count{ 0u };
        std::uint32_t instance_count{ 0u };
        std::uint32_t first_index{ 0u };
        std::int32_t vertex_offset{ 0 };
        std::uint32_t first_instance{ 0u };
    };

    struct compute_pipeline_create_info_t
    {
        std::string_view shader_path;
        std::string_view debug_name;
        std::uint32_t threadgroup_size_x{ 1u };
        std::uint32_t threadgroup_size_y{ 1u };
        std::uint32_t threadgroup_size_z{ 1u };
        std::uint32_t max_constant_size_bytes{ 0u };
    };

    struct compute_buffer_binding_t
    {
        std::uint32_t slot{ 0u };
        const rhi_buffer_t* buffer{ nullptr };
    };

    enum class compute_dispatch_order_t : std::uint8_t
    {
        before_graphics
    };

    enum class compute_graphics_handoff_t : std::uint8_t
    {
        none,
        storage_write_to_graphics_read
    };

    class rhi_compute_pipeline_t
    {
    public:
        explicit rhi_compute_pipeline_t(const compute_pipeline_create_info_t& info) noexcept
            : _info{ info } {}
        virtual ~rhi_compute_pipeline_t() = default;

        [[nodiscard]] const compute_pipeline_create_info_t& info() const noexcept { return _info; }

    private:
        compute_pipeline_create_info_t _info;
    };

    struct compute_dispatch_record_t
    {
        const rhi_compute_pipeline_t* pipeline{ nullptr };
        std::span<const compute_buffer_binding_t> read_only_buffers{ };
        std::span<const compute_buffer_binding_t> storage_buffers{ };
        std::span<const std::byte> constants{ };
        compute_dispatch_order_t order{ compute_dispatch_order_t::before_graphics };
        compute_graphics_handoff_t graphics_handoff{ compute_graphics_handoff_t::none };
        std::uint32_t group_count_x{ 1u };
        std::uint32_t group_count_y{ 1u };
        std::uint32_t group_count_z{ 1u };
    };
} // namespace carrot::rhi
