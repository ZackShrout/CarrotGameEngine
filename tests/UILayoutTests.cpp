//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "UI/UI.h"

#include <cmath>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        [[nodiscard]] bool nearly_equal(const float lhs, const float rhs) noexcept
        {
            return std::fabs(lhs - rhs) <= 0.0001f;
        }

        class layout_probe_widget_t final : public ui::ui_widget_t
        {
        public:
            layout_probe_widget_t() = default;

            explicit layout_probe_widget_t(const ui::ui_size_t size)
            {
                set_desired_size(size);
            }

            [[nodiscard]] std::string_view get_debug_name() const noexcept override { return "layout_probe_widget_t"; }

            uint32_t layout_update_count{ 0u };

        protected:
            void on_layout_updated(const ui::ui_rect_t& bounds) noexcept override
            {
                (void)bounds;
                ++layout_update_count;
            }
        };

        void test_vertical_stack_arranges_children_with_padding_and_spacing()
        {
            ui::ui_stack_t root{ ui::ui_stack_orientation_t::vertical };
            root.set_padding({ .left = 2.f, .top = 3.f, .right = 4.f, .bottom = 5.f });
            root.set_spacing(10.f);

            layout_probe_widget_t& child_a{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 50.f, 20.f }) };
            layout_probe_widget_t& child_b{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 80.f, 30.f }) };

            root.layout_tree({ .x = 0.f, .y = 0.f, .width = 200.f, .height = 120.f });

            const ui::ui_rect_t& a{ child_a.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(a.x, 2.f));
            CARROT_TEST_REQUIRE(nearly_equal(a.y, 3.f));
            CARROT_TEST_REQUIRE(nearly_equal(a.width, 194.f));
            CARROT_TEST_REQUIRE(nearly_equal(a.height, 20.f));

            const ui::ui_rect_t& b{ child_b.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(b.x, 2.f));
            CARROT_TEST_REQUIRE(nearly_equal(b.y, 33.f));
            CARROT_TEST_REQUIRE(nearly_equal(b.width, 194.f));
            CARROT_TEST_REQUIRE(nearly_equal(b.height, 30.f));
        }

        void test_vertical_stack_center_alignment_uses_desired_cross_size()
        {
            ui::ui_stack_t root{ ui::ui_stack_orientation_t::vertical };
            root.set_cross_alignment(ui::ui_stack_cross_alignment_t::center);

            layout_probe_widget_t& child{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 50.f, 20.f }) };

            root.layout_tree({ .x = 0.f, .y = 0.f, .width = 200.f, .height = 100.f });

            const ui::ui_rect_t& bounds{ child.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(bounds.x, 75.f));
            CARROT_TEST_REQUIRE(nearly_equal(bounds.y, 0.f));
            CARROT_TEST_REQUIRE(nearly_equal(bounds.width, 50.f));
            CARROT_TEST_REQUIRE(nearly_equal(bounds.height, 20.f));
        }

        void test_layout_invalidation_relayouts_only_dirty_subtree()
        {
            ui::ui_panel_t root;
            layout_probe_widget_t& dirty_child{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 25.f, 25.f }) };
            layout_probe_widget_t& stable_child{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 25.f, 25.f }) };

            const ui::ui_rect_t viewport{ .x = 0.f, .y = 0.f, .width = 300.f, .height = 200.f };
            root.layout_tree(viewport);

            CARROT_TEST_REQUIRE(dirty_child.layout_update_count == 1u);
            CARROT_TEST_REQUIRE(stable_child.layout_update_count == 1u);

            dirty_child.set_desired_size({ 60.f, 40.f });
            root.layout_tree(viewport);

            CARROT_TEST_REQUIRE(dirty_child.layout_update_count == 2u);
            CARROT_TEST_REQUIRE(stable_child.layout_update_count == 1u);
        }

        void test_vertical_stack_distributes_remaining_space_to_flex_children()
        {
            ui::ui_stack_t root{ ui::ui_stack_orientation_t::vertical };
            root.set_spacing(10.f);

            layout_probe_widget_t& fixed{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 100.f, 20.f }) };
            layout_probe_widget_t& flex_a{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 100.f, 0.f }) };
            layout_probe_widget_t& flex_b{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 100.f, 0.f }) };

            flex_a.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);
            flex_b.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);
            flex_a.set_flex_weight(1.f);
            flex_b.set_flex_weight(3.f);

            root.layout_tree({ .x = 0.f, .y = 0.f, .width = 200.f, .height = 200.f });

            const ui::ui_rect_t& fixed_bounds{ fixed.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(fixed_bounds.y, 0.f));
            CARROT_TEST_REQUIRE(nearly_equal(fixed_bounds.height, 20.f));

            const ui::ui_rect_t& a_bounds{ flex_a.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(a_bounds.y, 30.f));
            CARROT_TEST_REQUIRE(nearly_equal(a_bounds.height, 40.f));

            const ui::ui_rect_t& b_bounds{ flex_b.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(b_bounds.y, 80.f));
            CARROT_TEST_REQUIRE(nearly_equal(b_bounds.height, 120.f));
        }

        void test_horizontal_stack_flex_weight_splits_remaining_width()
        {
            ui::ui_stack_t root{ ui::ui_stack_orientation_t::horizontal };
            root.set_spacing(5.f);
            root.set_cross_alignment(ui::ui_stack_cross_alignment_t::stretch);

            layout_probe_widget_t& left{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 40.f, 10.f }) };
            layout_probe_widget_t& center{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 0.f, 10.f }) };
            layout_probe_widget_t& right{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 0.f, 10.f }) };

            center.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);
            right.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);
            center.set_flex_weight(1.f);
            right.set_flex_weight(1.f);

            root.layout_tree({ .x = 0.f, .y = 0.f, .width = 200.f, .height = 50.f });

            const ui::ui_rect_t& left_bounds{ left.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(left_bounds.x, 0.f));
            CARROT_TEST_REQUIRE(nearly_equal(left_bounds.width, 40.f));

            const ui::ui_rect_t& center_bounds{ center.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(center_bounds.x, 45.f));
            CARROT_TEST_REQUIRE(nearly_equal(center_bounds.width, 75.f));

            const ui::ui_rect_t& right_bounds{ right.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(right_bounds.x, 125.f));
            CARROT_TEST_REQUIRE(nearly_equal(right_bounds.width, 75.f));
        }

        void test_stack_respects_child_min_size_constraints()
        {
            ui::ui_stack_t root{ ui::ui_stack_orientation_t::vertical };
            root.set_cross_alignment(ui::ui_stack_cross_alignment_t::start);
            layout_probe_widget_t& child{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 10.f, 10.f }) };
            child.set_min_size({ 30.f, 40.f });

            root.layout_tree({ .x = 0.f, .y = 0.f, .width = 200.f, .height = 200.f });

            const ui::ui_rect_t& bounds{ child.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(bounds.width, 30.f));
            CARROT_TEST_REQUIRE(nearly_equal(bounds.height, 40.f));
        }

        void test_stack_respects_child_max_size_constraints_for_flex()
        {
            ui::ui_stack_t root{ ui::ui_stack_orientation_t::horizontal };
            layout_probe_widget_t& child{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 0.f, 20.f }) };

            child.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);
            child.set_flex_weight(1.f);
            child.set_max_size({ 50.f, 200.f });

            root.layout_tree({ .x = 0.f, .y = 0.f, .width = 300.f, .height = 100.f });

            const ui::ui_rect_t& bounds{ child.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(bounds.width, 50.f));
        }

        void test_flex_redistributes_when_a_child_hits_max_size()
        {
            ui::ui_stack_t root{ ui::ui_stack_orientation_t::horizontal };

            layout_probe_widget_t& flex_a{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 0.f, 20.f }) };
            layout_probe_widget_t& flex_b{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 0.f, 20.f }) };

            flex_a.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);
            flex_b.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);
            flex_a.set_flex_weight(1.f);
            flex_b.set_flex_weight(1.f);
            flex_a.set_max_size({ 50.f, 200.f });

            root.layout_tree({ .x = 0.f, .y = 0.f, .width = 200.f, .height = 100.f });

            const ui::ui_rect_t& a_bounds{ flex_a.get_layout_bounds() };
            const ui::ui_rect_t& b_bounds{ flex_b.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(a_bounds.width, 50.f));
            CARROT_TEST_REQUIRE(nearly_equal(b_bounds.width, 150.f));
        }

        void test_min_overflow_policy_preserves_minimum_sizes()
        {
            ui::ui_stack_t root{ ui::ui_stack_orientation_t::vertical };
            root.set_spacing(10.f);
            root.set_cross_alignment(ui::ui_stack_cross_alignment_t::start);

            layout_probe_widget_t& first{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 20.f, 10.f }) };
            layout_probe_widget_t& second{ root.emplace_child<layout_probe_widget_t>(ui::ui_size_t{ 20.f, 10.f }) };
            first.set_min_size({ 20.f, 50.f });
            second.set_min_size({ 20.f, 50.f });

            root.layout_tree({ .x = 0.f, .y = 0.f, .width = 120.f, .height = 60.f });

            const ui::ui_rect_t& first_bounds{ first.get_layout_bounds() };
            const ui::ui_rect_t& second_bounds{ second.get_layout_bounds() };
            CARROT_TEST_REQUIRE(nearly_equal(first_bounds.height, 50.f));
            CARROT_TEST_REQUIRE(nearly_equal(second_bounds.y, 60.f));
            CARROT_TEST_REQUIRE(nearly_equal(second_bounds.height, 50.f));
        }
    } // namespace

    void register_ui_layout_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("ui layout vertical stack uses padding and spacing",
                           test_vertical_stack_arranges_children_with_padding_and_spacing);
        tests.emplace_back("ui layout vertical stack center alignment uses desired width",
                           test_vertical_stack_center_alignment_uses_desired_cross_size);
        tests.emplace_back("ui layout invalidation relayouts only dirty subtree",
                           test_layout_invalidation_relayouts_only_dirty_subtree);
        tests.emplace_back("ui layout vertical stack flex children consume remaining space",
                           test_vertical_stack_distributes_remaining_space_to_flex_children);
        tests.emplace_back("ui layout horizontal stack flex children split remaining width",
                           test_horizontal_stack_flex_weight_splits_remaining_width);
        tests.emplace_back("ui layout stack honors child min size constraints",
                           test_stack_respects_child_min_size_constraints);
        tests.emplace_back("ui layout stack honors child max size constraints for flex",
                           test_stack_respects_child_max_size_constraints_for_flex);
        tests.emplace_back("ui layout flex redistributes when sibling hits max",
                           test_flex_redistributes_when_a_child_hits_max_size);
        tests.emplace_back("ui layout min overflow policy preserves minimum sizes",
                           test_min_overflow_policy_preserves_minimum_sizes);
    }
} // namespace carrot::tests
