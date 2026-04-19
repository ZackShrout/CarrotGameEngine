//
// Created by Zack Shrout on 4/19/2026.
//

#include "TestCommon.h"

#include "Renderer/Draw/TexturedQuadCameraUniform.h"

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        void test_pack_world_forward_plus_uniform_preserves_split_inputs()
        {
            using namespace carrot::renderer;

            forward_plus_frame_constants_t constants{ };
            constants.grid_params = { 10.f, 20.f, 128.f, 0.f };
            constants.tile_counts = { 4u, 3u, 128u, 0u };
            constants.point_light_counts = { 2u, 0u, 0u, 0u };

            forward_plus_light_input_t light_input{ };
            light_input.point_lights[0].position_radius_px = { 32.f, 48.f, 64.f, 0.f };
            light_input.point_lights[1].color_intensity = { 0.25f, 0.5f, 0.75f, 1.5f };

            forward_plus_classification_output_t output{ };
            output.tile_headers[5].light_index_offset = 9u;
            output.tile_headers[5].light_count = 3u;
            output.packed_light_indices[2] = packed_uint4_t{ .x = 7u, .y = 8u, .z = 9u, .w = 10u };

            const world_forward_plus_uniform_t packed{
                pack_world_forward_plus_uniform(chlm::float4x4::identity(),
                                               chlm::float4{ 1.f, 0.8f, 0.6f, 1.f },
                                               constants,
                                               light_input,
                                               output)
            };

            CARROT_TEST_REQUIRE(packed.forward_plus_constants.grid_params.x == 10.f);
            CARROT_TEST_REQUIRE(packed.forward_plus_constants.tile_counts[0] == 4u);
            CARROT_TEST_REQUIRE(packed.forward_plus_constants.point_light_counts[0] == 2u);
            CARROT_TEST_REQUIRE(packed.point_lights[0].position_radius_px.z == 64.f);
            CARROT_TEST_REQUIRE(packed.point_lights[1].color_intensity.w == 1.5f);
            CARROT_TEST_REQUIRE(packed.forward_plus_tiles[5].light_index_offset == 9u);
            CARROT_TEST_REQUIRE(packed.forward_plus_tiles[5].light_count == 3u);
            CARROT_TEST_REQUIRE(packed.forward_plus_light_indices[2].z == 9u);
        }
    } // namespace

    void register_forward_plus_data_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("forward plus pack preserves split inputs",
                           test_pack_world_forward_plus_uniform_preserves_split_inputs);
    }
} // namespace carrot::tests
