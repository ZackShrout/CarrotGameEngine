//
// Created by Zack Shrout on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Collision/CollisionWorld.h"
#include "TestCommon.h"
#include "World/World.h"

#include <cmath>
#include <functional>
#include <string_view>
#include <vector>

namespace carrot::tests {
    namespace {
        [[nodiscard]] bool nearly_equal(const float a, const float b, const float epsilon = 1.0e-4f) noexcept
        {
            return std::fabs(a - b) <= epsilon;
        }

        [[nodiscard]] collision::collision_aabb_t unit_box_at(const float x, const float y) noexcept
        {
            return collision::collision_aabb_t::from_min_size(chlm::float2{ x, y }, chlm::float2{ 1.f, 1.f });
        }

        [[nodiscard]] collision::static_collider_t diamond_collider() noexcept
        {
            return collision::static_collider_t{
                .shape = collision::static_collider_t::shape_t::convex_polygon,
                .polygon_points = {
                    chlm::float2{ 2.f, 1.f },
                    chlm::float2{ 3.f, 0.f },
                    chlm::float2{ 4.f, 1.f },
                    chlm::float2{ 3.f, 2.f }
                },
                .layer = collision::make_collision_layer(0u),
                .mask = collision::k_collision_mask_all
            };
        }

        void test_collision_world_point_query_respects_filters()
        {
            collision::collision_world_t world;
            const auto& first = world.add_static_collider(collision::static_collider_t{
                .bounds = unit_box_at(2.f, 2.f),
                .layer = collision::make_collision_layer(0u),
                .mask = collision::k_collision_mask_all
            });
            const auto& second = world.add_static_collider(collision::static_collider_t{
                .bounds = unit_box_at(2.f, 2.f),
                .layer = collision::make_collision_layer(1u),
                .mask = collision::k_collision_mask_all
            });
            static_cast<void>(first);
            static_cast<void>(second);

            const auto hits_all{ world.point_query(chlm::float2{ 2.5f, 2.5f }) };
            CARROT_TEST_REQUIRE(hits_all.size() == 2u);

            const auto hits_filtered{ world.point_query(chlm::float2{ 2.5f, 2.5f },
                                                        collision::collision_query_filter_t{
                                                            .layer = collision::make_collision_layer(0u),
                                                            .mask = collision::make_collision_layer(0u)
                                                        }) };
            CARROT_TEST_REQUIRE(hits_filtered.size() == 1u);
            CARROT_TEST_REQUIRE(hits_filtered.front().is_static_collider());
            CARROT_TEST_REQUIRE(hits_filtered.front().layer == collision::make_collision_layer(0u));
        }

        void test_collision_world_overlap_query_returns_static_and_tile_hits()
        {
            collision::collision_world_t world;
            const auto& collider = world.add_static_collider(collision::static_collider_t{
                .bounds = unit_box_at(0.f, 0.f),
                .layer = collision::make_collision_layer(0u),
                .mask = collision::k_collision_mask_all
            });
            static_cast<void>(collider);

            auto& tile_field{ world.create_tile_collision_field(3u, 2u, chlm::float2{ 0.f, 0.f }, chlm::float2{ 1.f, 1.f }) };
            tile_field.cell_at(2u, 1u)->solid = true;
            tile_field.cell_at(2u, 1u)->layer = collision::make_collision_layer(0u);

            const auto hits{ world.overlap_query(collision::collision_aabb_t::from_min_size(chlm::float2{ 0.5f, 0.5f },
                                                                                            chlm::float2{ 2.2f, 1.1f })) };
            CARROT_TEST_REQUIRE(hits.size() == 2u);

            bool found_static{ false };
            bool found_tile{ false };

            for (const collision::collision_hit_ref_t& hit : hits)
            {
                found_static = found_static || hit.is_static_collider();
                found_tile = found_tile || (hit.is_tile_cell() && hit.tile_x == 2u && hit.tile_y == 1u);
            }

            CARROT_TEST_REQUIRE(found_static);
            CARROT_TEST_REQUIRE(found_tile);
        }

        void test_collision_world_raycast_hits_nearest_blocker()
        {
            collision::collision_world_t world;
            const auto& near = world.add_static_collider(collision::static_collider_t{
                .bounds = unit_box_at(4.f, 0.f),
                .layer = collision::make_collision_layer(0u),
                .mask = collision::k_collision_mask_all
            });
            const auto& far = world.add_static_collider(collision::static_collider_t{
                .bounds = unit_box_at(7.f, 0.f),
                .layer = collision::make_collision_layer(0u),
                .mask = collision::k_collision_mask_all
            });
            static_cast<void>(near);
            static_cast<void>(far);

            const auto hit{ world.raycast(chlm::float2{ 0.f, 0.5f }, chlm::float2{ 10.f, 0.f }) };
            CARROT_TEST_REQUIRE(hit.has_value());
            CARROT_TEST_REQUIRE(hit->hit.is_static_collider());
            CARROT_TEST_REQUIRE(nearly_equal(hit->fraction, 0.4f));
            CARROT_TEST_REQUIRE(nearly_equal(hit->distance, 4.f));
            CARROT_TEST_REQUIRE(nearly_equal(hit->position.x, 4.f));
            CARROT_TEST_REQUIRE(hit->normal.x == -1.f);
            CARROT_TEST_REQUIRE(hit->normal.y == 0.f);
        }

        void test_collision_world_sweep_hits_tiles_and_reports_contact_fraction()
        {
            collision::collision_world_t world;
            auto& tile_field{ world.create_tile_collision_field(4u, 1u, chlm::float2{ 0.f, 0.f }, chlm::float2{ 1.f, 1.f }) };
            tile_field.cell_at(2u, 0u)->solid = true;
            tile_field.cell_at(2u, 0u)->layer = collision::make_collision_layer(0u);

            const collision::collision_aabb_t moving{ collision::collision_aabb_t::from_min_size(chlm::float2{ 0.f, 0.f },
                                                                                                  chlm::float2{ 1.f, 1.f }) };

            const auto hit{ world.sweep_aabb(moving, chlm::float2{ 3.f, 0.f }) };
            CARROT_TEST_REQUIRE(hit.has_value());
            CARROT_TEST_REQUIRE(hit->hit.is_tile_cell());
            CARROT_TEST_REQUIRE(hit->hit.tile_x == 2u);
            CARROT_TEST_REQUIRE(hit->hit.tile_y == 0u);
            CARROT_TEST_REQUIRE(!hit->started_overlapping);
            CARROT_TEST_REQUIRE(nearly_equal(hit->fraction, 1.f / 3.f));
            CARROT_TEST_REQUIRE(nearly_equal(hit->distance, 1.f));
            CARROT_TEST_REQUIRE(hit->normal.x == -1.f);
        }

        void test_collision_world_sweep_reports_initial_overlap()
        {
            collision::collision_world_t world;
            const auto& collider = world.add_static_collider(collision::static_collider_t{
                .bounds = unit_box_at(0.f, 0.f),
                .layer = collision::make_collision_layer(0u),
                .mask = collision::k_collision_mask_all
            });
            static_cast<void>(collider);

            const auto hit{ world.sweep_aabb(collision::collision_aabb_t::from_min_size(chlm::float2{ 0.25f, 0.25f },
                                                                                        chlm::float2{ 0.5f, 0.5f }),
                                             chlm::float2{ 2.f, 0.f }) };
            CARROT_TEST_REQUIRE(hit.has_value());
            CARROT_TEST_REQUIRE(hit->started_overlapping);
            CARROT_TEST_REQUIRE(nearly_equal(hit->fraction, 0.f));
            CARROT_TEST_REQUIRE(nearly_equal(hit->distance, 0.f));
        }

        void test_collision_world_point_query_hits_convex_polygon()
        {
            collision::collision_world_t world;
            (void)world.add_static_collider(diamond_collider());

            const auto hits{ world.point_query(chlm::float2{ 3.f, 1.f }) };
            CARROT_TEST_REQUIRE(hits.size() == 1u);
            CARROT_TEST_REQUIRE(hits.front().is_static_collider());
            CARROT_TEST_REQUIRE(hits.front().collider->is_convex_polygon());
        }

        void test_collision_world_sweep_hits_convex_polygon()
        {
            collision::collision_world_t world;
            (void)world.add_static_collider(diamond_collider());

            const collision::collision_aabb_t moving{
                collision::collision_aabb_t::from_min_size(chlm::float2{ 0.f, 0.5f }, chlm::float2{ 1.f, 1.f })
            };

            const auto hit{ world.sweep_aabb(moving, chlm::float2{ 3.f, 0.f }) };
            CARROT_TEST_REQUIRE(hit.has_value());
            CARROT_TEST_REQUIRE(hit->hit.is_static_collider());
            CARROT_TEST_REQUIRE(hit->hit.collider->is_convex_polygon());
            CARROT_TEST_REQUIRE(!hit->started_overlapping);
            CARROT_TEST_REQUIRE(nearly_equal(hit->fraction, 1.f / 3.f));
            CARROT_TEST_REQUIRE(nearly_equal(hit->distance, 1.f));
            CARROT_TEST_REQUIRE(nearly_equal(hit->normal.x, -1.f));
            CARROT_TEST_REQUIRE(nearly_equal(hit->normal.y, 0.f));
        }

        void test_collision_world_sweep_allows_separating_from_touching_contact()
        {
            collision::collision_world_t world;
            const auto& collider = world.add_static_collider(collision::static_collider_t{
                .bounds = unit_box_at(1.f, 0.f),
                .layer = collision::make_collision_layer(0u),
                .mask = collision::k_collision_mask_all
            });
            static_cast<void>(collider);

            const collision::collision_aabb_t touching{ collision::collision_aabb_t::from_min_size(chlm::float2{ 0.f, 0.f },
                                                                                                    chlm::float2{ 1.f, 1.f }) };

            const auto hit{ world.sweep_aabb(touching, chlm::float2{ -1.f, 0.f }) };
            CARROT_TEST_REQUIRE(!hit.has_value());
        }

        void test_world_clear_resets_collision_world()
        {
            world::world_t world;
            const auto& collider = world.collision_world().add_static_collider(collision::static_collider_t{
                .bounds = unit_box_at(1.f, 1.f),
                .layer = collision::make_collision_layer(0u),
                .mask = collision::k_collision_mask_all
            });
            static_cast<void>(collider);

            CARROT_TEST_REQUIRE(world.collision_world().static_colliders().size() == 1u);
            world.clear();
            CARROT_TEST_REQUIRE(world.collision_world().static_colliders().empty());
        }
    } // namespace

    void register_collision_world_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("collision world point query respects filters", test_collision_world_point_query_respects_filters);
        tests.emplace_back("collision world overlap query returns static and tile hits",
                           test_collision_world_overlap_query_returns_static_and_tile_hits);
        tests.emplace_back("collision world raycast hits nearest blocker", test_collision_world_raycast_hits_nearest_blocker);
        tests.emplace_back("collision world sweep hits tiles and reports contact fraction",
                           test_collision_world_sweep_hits_tiles_and_reports_contact_fraction);
        tests.emplace_back("collision world sweep reports initial overlap", test_collision_world_sweep_reports_initial_overlap);
        tests.emplace_back("collision world point query hits convex polygon",
                           test_collision_world_point_query_hits_convex_polygon);
        tests.emplace_back("collision world sweep hits convex polygon",
                           test_collision_world_sweep_hits_convex_polygon);
        tests.emplace_back("collision world sweep allows separating from touching contact",
                           test_collision_world_sweep_allows_separating_from_touching_contact);
        tests.emplace_back("world clear resets collision world", test_world_clear_resets_collision_world);
    }
} // namespace carrot::tests
