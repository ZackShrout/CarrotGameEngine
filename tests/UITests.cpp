//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "UI/UI.h"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        class counting_widget_t final : public ui::ui_widget_t
        {
        public:
            [[nodiscard]] std::string_view get_debug_name() const noexcept override { return "counting_widget_t"; }

            uint32_t tick_count{ 0u };
            uint32_t attach_count{ 0u };
            uint32_t detach_count{ 0u };

        protected:
            void on_tick(const float delta_time) noexcept override
            {
                (void)delta_time;
                ++tick_count;
            }

            void on_attached_to_tree() noexcept override
            {
                ++attach_count;
            }

            void on_detached_from_tree() noexcept override
            {
                ++detach_count;
            }
        };

        void test_ui_widget_tree_composition_supports_nested_panels()
        {
            ui::ui_root_widget_t root;
            ui::ui_panel_t& panel_a{ root.emplace_child<ui::ui_panel_t>() };
            ui::ui_panel_t& panel_b{ panel_a.emplace_child<ui::ui_panel_t>() };

            CARROT_TEST_REQUIRE(root.get_children().size() == 1u);
            CARROT_TEST_REQUIRE(root.get_children().front().get() == &panel_a);
            CARROT_TEST_REQUIRE(panel_a.get_parent() == &root);

            CARROT_TEST_REQUIRE(panel_a.get_children().size() == 1u);
            CARROT_TEST_REQUIRE(panel_a.get_children().front().get() == &panel_b);
            CARROT_TEST_REQUIRE(panel_b.get_parent() == &panel_a);
        }

        void test_ui_widget_tick_tree_updates_entire_hierarchy()
        {
            counting_widget_t root;
            counting_widget_t& child{ root.emplace_child<counting_widget_t>() };
            counting_widget_t& grandchild{ child.emplace_child<counting_widget_t>() };

            root.tick_tree(1.f / 60.f);

            CARROT_TEST_REQUIRE(root.tick_count == 1u);
            CARROT_TEST_REQUIRE(child.tick_count == 1u);
            CARROT_TEST_REQUIRE(grandchild.tick_count == 1u);
        }

        void test_ui_widget_attach_detach_propagates_to_children()
        {
            counting_widget_t root;
            counting_widget_t& child{ root.emplace_child<counting_widget_t>() };
            counting_widget_t& grandchild{ child.emplace_child<counting_widget_t>() };

            root.attach_to_tree();
            CARROT_TEST_REQUIRE(root.attach_count == 1u);
            CARROT_TEST_REQUIRE(child.attach_count == 1u);
            CARROT_TEST_REQUIRE(grandchild.attach_count == 1u);

            root.detach_from_tree();
            CARROT_TEST_REQUIRE(root.detach_count == 1u);
            CARROT_TEST_REQUIRE(child.detach_count == 1u);
            CARROT_TEST_REQUIRE(grandchild.detach_count == 1u);
        }

        void test_ui_widget_remove_child_clears_parent()
        {
            ui::ui_root_widget_t root;
            ui::ui_panel_t& panel{ root.emplace_child<ui::ui_panel_t>() };

            std::unique_ptr<ui::ui_widget_t> removed{ root.remove_child(panel) };

            CARROT_TEST_REQUIRE(removed != nullptr);
            CARROT_TEST_REQUIRE(root.get_children().empty());
            CARROT_TEST_REQUIRE(removed->get_parent() == nullptr);
        }
    } // namespace

    void register_ui_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("ui widget tree supports nested panels", test_ui_widget_tree_composition_supports_nested_panels);
        tests.emplace_back("ui widget tick tree updates hierarchy", test_ui_widget_tick_tree_updates_entire_hierarchy);
        tests.emplace_back("ui widget attach/detach propagates through hierarchy", test_ui_widget_attach_detach_propagates_to_children);
        tests.emplace_back("ui widget remove_child clears parent linkage", test_ui_widget_remove_child_clears_parent);
    }
} // namespace carrot::tests
