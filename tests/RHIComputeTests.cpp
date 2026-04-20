//
// Created by Zack Shrout on 4/18/2026.
//

#include "TestCommon.h"

#include "RHI/Backends/Null/NullRHIContext.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        void test_null_rhi_records_compute_dispatches()
        {
            using namespace carrot::rhi;

            null::null_rhi_context_t context{
                rhi_desc_t{
                    .api = graphics_api::null_backend,
                    .enable_debug_layers = false
                }
            };

            const auto pipeline{
                context.create_compute_pipeline({
                    .shader_path = "engine://shaders/null/compute_buffer_fill.fake",
                    .debug_name = "null compute smoke",
                    .threadgroup_size_x = 64u,
                    .max_constant_size_bytes = 16u
                })
            };
            CARROT_TEST_REQUIRE(pipeline != nullptr);

            const std::array<std::uint32_t, 4> data{ 0u, 1u, 2u, 3u };
            const auto storage_buffer{
                context.create_buffer({
                    .size_bytes = sizeof(data),
                    .usage = buffer_usage_t::storage,
                    .initial_data = data.data()
                })
            };
            CARROT_TEST_REQUIRE(storage_buffer != nullptr);

            const std::array<std::uint32_t, 4> constants{ 7u, 4u, 0u, 0u };
            const std::array<compute_buffer_binding_t, 1> storage_bindings{ compute_buffer_binding_t{
                .slot = 0u,
                .buffer = storage_buffer.get()
            } };

            context.begin_frame();
            context.dispatch_compute({
                .pipeline = pipeline.get(),
                .storage_buffers = storage_bindings,
                .constants = std::as_bytes(std::span{ constants }),
                .graphics_handoff = compute_graphics_handoff_t::storage_write_to_graphics_read,
                .group_count_x = 2u,
                .group_count_y = 1u,
                .group_count_z = 1u
            });
            context.end_frame();

            const auto& dispatches{ context.recorded_compute_dispatches() };
            CARROT_TEST_REQUIRE(dispatches.size() == 1u);
            CARROT_TEST_REQUIRE(dispatches[0].debug_name == "null compute smoke");
            CARROT_TEST_REQUIRE(dispatches[0].read_only_buffer_count == 0u);
            CARROT_TEST_REQUIRE(dispatches[0].storage_buffer_count == 1u);
            CARROT_TEST_REQUIRE(dispatches[0].constant_size_bytes == sizeof(constants));
            CARROT_TEST_REQUIRE(dispatches[0].order == compute_dispatch_order_t::before_graphics);
            CARROT_TEST_REQUIRE(dispatches[0].graphics_handoff ==
                                compute_graphics_handoff_t::storage_write_to_graphics_read);
            CARROT_TEST_REQUIRE(dispatches[0].group_count_x == 2u);
        }

        void test_null_rhi_records_indirect_textured_quad_stage()
        {
            using namespace carrot::rhi;

            null::null_rhi_context_t context{
                rhi_desc_t{
                    .api = graphics_api::null_backend,
                    .enable_debug_layers = false
                }
            };

            const auto vertex_buffer{
                context.create_buffer({
                    .size_bytes = 64u,
                    .usage = buffer_usage_t::vertex,
                    .cpu_writable = true
                })
            };
            const auto index_buffer{
                context.create_buffer({
                    .size_bytes = 24u,
                    .usage = buffer_usage_t::index,
                    .cpu_writable = true
                })
            };
            const indexed_indirect_draw_command_t command{
                .index_count = 6u,
                .instance_count = 1u,
                .first_index = 0u,
                .vertex_offset = 0,
                .first_instance = 0u
            };
            const auto indirect_buffer{
                context.create_buffer({
                    .size_bytes = sizeof(command),
                    .usage = buffer_usage_t::indirect,
                    .initial_data = &command
                })
            };
            const auto texture{
                context.create_texture_2d({
                    .width = 4u,
                    .height = 4u,
                    .format = texture_format_t::rgba8_srgb
                })
            };
            const auto sampler{
                context.create_sampler(sampler_desc_t{ })
            };

            CARROT_TEST_REQUIRE(vertex_buffer != nullptr);
            CARROT_TEST_REQUIRE(index_buffer != nullptr);
            CARROT_TEST_REQUIRE(indirect_buffer != nullptr);
            CARROT_TEST_REQUIRE(texture != nullptr);
            CARROT_TEST_REQUIRE(sampler != nullptr);

            context.begin_frame();
            context.record_indirect_textured_quad_stage({
                .vertex_buffer = vertex_buffer.get(),
                .index_buffer = index_buffer.get(),
                .indirect_buffer = indirect_buffer.get(),
                .texture = texture.get(),
                .sampler = sampler.get(),
                .ambient_color = { 0.25f, 0.5f, 0.75f, 1.f },
                .forward_plus_constants = {
                    .point_light_counts = { 3u, 0u, 0u, 0u }
                },
                .viewport = render_viewport_t{
                    .rect_px = {
                        .position = { 8u, 12u },
                        .size = { 320u, 180u }
                    }
                },
                .indirect_buffer_offset_bytes = 16u,
                .presentation_mask = presentation_channel_log_console
            });
            context.end_frame();

            const auto& stages{ context.recorded_indirect_textured_stages() };
            CARROT_TEST_REQUIRE(stages.size() == 1u);
            CARROT_TEST_REQUIRE(stages[0].viewport.rect_px.position.x == 8u);
            CARROT_TEST_REQUIRE(stages[0].viewport.rect_px.position.y == 12u);
            CARROT_TEST_REQUIRE(stages[0].viewport.rect_px.size.x == 320u);
            CARROT_TEST_REQUIRE(stages[0].viewport.rect_px.size.y == 180u);
            CARROT_TEST_REQUIRE(stages[0].presentation_mask == presentation_channel_log_console);
            CARROT_TEST_REQUIRE(stages[0].point_light_count == 3u);
            CARROT_TEST_REQUIRE(stages[0].indirect_buffer_offset_bytes == 16u);
        }
    } // namespace

    void register_rhi_compute_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("null rhi records compute dispatches", test_null_rhi_records_compute_dispatches);
        tests.emplace_back("null rhi records indirect textured quad stage", test_null_rhi_records_indirect_textured_quad_stage);
    }
} // namespace carrot::tests
