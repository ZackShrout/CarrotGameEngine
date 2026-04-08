//
// Created by Zack Shrout on 4/8/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "UI/UIModule.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIFocusScope.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        class navigation_probe_widget_t final : public ui::ui_widget_t
        {
        public:
            explicit navigation_probe_widget_t(const bool focusable = true)
            {
                set_focusable(focusable);
            }

            [[nodiscard]] std::string_view get_debug_name() const noexcept override { return "navigation_probe_widget_t"; }

            uint32_t focus_gained_count{ 0u };
            uint32_t focus_lost_count{ 0u };
            uint32_t accept_count{ 0u };
            uint32_t cancel_count{ 0u };

        protected:
            void on_focus_gained() noexcept override
            {
                ++focus_gained_count;
            }

            void on_focus_lost() noexcept override
            {
                ++focus_lost_count;
            }

            [[nodiscard]] bool on_ui_accept() noexcept override
            {
                ++accept_count;
                return true;
            }

            [[nodiscard]] bool on_ui_cancel() noexcept override
            {
                ++cancel_count;
                return true;
            }
        };

        void test_ui_module_focus_traversal_skips_non_focusable_widgets()
        {
            ui::ui_module_t module;
            module.init();
            module.set_focus_looping_enabled(false);

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            navigation_probe_widget_t& first{ root->emplace_child<navigation_probe_widget_t>(true) };
            root->emplace_child<navigation_probe_widget_t>(false);
            navigation_probe_widget_t& third{ root->emplace_child<navigation_probe_widget_t>(true) };

            CARROT_TEST_REQUIRE(module.focus_first());
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &first);
            CARROT_TEST_REQUIRE(first.focus_gained_count == 1u);

            CARROT_TEST_REQUIRE(module.focus_next());
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &third);
            CARROT_TEST_REQUIRE(first.focus_lost_count == 1u);
            CARROT_TEST_REQUIRE(third.focus_gained_count == 1u);

            CARROT_TEST_REQUIRE(module.focus_previous());
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &first);

            CARROT_TEST_REQUIRE(!module.focus_previous());
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &first);
        }

        void test_ui_module_focus_traversal_loops_by_default()
        {
            ui::ui_module_t module;
            module.init();

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            navigation_probe_widget_t& first{ root->emplace_child<navigation_probe_widget_t>(true) };
            navigation_probe_widget_t& second{ root->emplace_child<navigation_probe_widget_t>(true) };
            navigation_probe_widget_t& third{ root->emplace_child<navigation_probe_widget_t>(true) };

            CARROT_TEST_REQUIRE(module.set_focus(&third));
            CARROT_TEST_REQUIRE(module.focus_next());
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &first);

            CARROT_TEST_REQUIRE(module.focus_previous());
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &third);

            (void)second;
        }

        void test_ui_module_navigation_respects_explicit_directional_targets()
        {
            ui::ui_module_t module;
            module.init();

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            navigation_probe_widget_t& first{ root->emplace_child<navigation_probe_widget_t>(true) };
            navigation_probe_widget_t& second{ root->emplace_child<navigation_probe_widget_t>(true) };
            navigation_probe_widget_t& third{ root->emplace_child<navigation_probe_widget_t>(true) };

            first.set_navigation_target(ui::ui_navigation_direction_t::right, &third);

            CARROT_TEST_REQUIRE(module.set_focus(&first));
            CARROT_TEST_REQUIRE(module.navigate(ui::ui_navigation_direction_t::right));
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &third);

            CARROT_TEST_REQUIRE(module.navigate(ui::ui_navigation_direction_t::left));
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &second);
        }

        void test_ui_module_accept_cancel_and_action_name_routing()
        {
            ui::ui_module_t module;
            module.init();

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            navigation_probe_widget_t& widget{ root->emplace_child<navigation_probe_widget_t>(true) };
            CARROT_TEST_REQUIRE(module.set_focus(&widget));

            CARROT_TEST_REQUIRE(module.handle_navigation_action(ui::ui_navigation_action_t::accept));
            CARROT_TEST_REQUIRE(module.handle_navigation_action("ui_cancel"));
            CARROT_TEST_REQUIRE(widget.accept_count == 1u);
            CARROT_TEST_REQUIRE(widget.cancel_count == 1u);

            CARROT_TEST_REQUIRE(!module.handle_navigation_action("ui_unknown"));
        }

        void test_ui_module_clears_focus_when_focused_widget_leaves_tree()
        {
            ui::ui_module_t module;
            module.init();

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            navigation_probe_widget_t& widget{ root->emplace_child<navigation_probe_widget_t>(true) };
            CARROT_TEST_REQUIRE(module.set_focus(&widget));
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &widget);

            std::unique_ptr<ui::ui_widget_t> removed{ root->remove_child(widget) };
            CARROT_TEST_REQUIRE(removed != nullptr);

            module.update(1.f / 60.f);
            CARROT_TEST_REQUIRE(module.get_focused_widget() == nullptr);
        }

        void test_ui_button_focus_accept_cancel_callbacks_fire()
        {
            ui::ui_button_t button{ "Play" };

            uint32_t focused_count{ 0u };
            uint32_t focus_lost_count{ 0u };
            uint32_t pressed_count{ 0u };
            uint32_t canceled_count{ 0u };

            button.set_on_focused([&focused_count]() { ++focused_count; });
            button.set_on_focus_lost([&focus_lost_count]() { ++focus_lost_count; });
            button.set_on_pressed([&pressed_count]() { ++pressed_count; });
            button.set_on_canceled([&canceled_count]() { ++canceled_count; });

            button.notify_focus_gained();
            button.notify_focus_lost();
            CARROT_TEST_REQUIRE(button.dispatch_ui_accept());
            CARROT_TEST_REQUIRE(button.dispatch_ui_cancel());

            CARROT_TEST_REQUIRE(focused_count == 1u);
            CARROT_TEST_REQUIRE(focus_lost_count == 1u);
            CARROT_TEST_REQUIRE(pressed_count == 1u);
            CARROT_TEST_REQUIRE(canceled_count == 1u);
        }

        void test_ui_focus_scope_traps_focus_and_restores_on_hide()
        {
            ui::ui_module_t module;
            module.init();
            module.set_focus_looping_enabled(false);

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            navigation_probe_widget_t& outside{ root->emplace_child<navigation_probe_widget_t>(true) };
            ui::ui_focus_scope_t& scope{ root->emplace_child<ui::ui_focus_scope_t>() };
            scope.set_visibility(ui::ui_widget_visibility_t::collapsed);
            scope.set_focus_on_show_policy(ui::ui_focus_on_show_policy_t::first);
            scope.set_focus_trap_enabled(true);

            navigation_probe_widget_t& scoped_a{ scope.emplace_child<navigation_probe_widget_t>(true) };
            navigation_probe_widget_t& scoped_b{ scope.emplace_child<navigation_probe_widget_t>(true) };

            CARROT_TEST_REQUIRE(module.set_focus(&outside));

            scope.set_visibility(ui::ui_widget_visibility_t::visible);
            module.update(1.f / 60.f);
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &scoped_a);

            CARROT_TEST_REQUIRE(module.focus_next());
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &scoped_b);
            CARROT_TEST_REQUIRE(!module.focus_next());
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &scoped_b);

            scope.set_visibility(ui::ui_widget_visibility_t::collapsed);
            module.update(1.f / 60.f);
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &outside);
        }

        void test_ui_focus_scope_show_policy_last_restores_previous_focus()
        {
            ui::ui_module_t module;
            module.init();
            module.set_focus_looping_enabled(true);

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            navigation_probe_widget_t& outside{ root->emplace_child<navigation_probe_widget_t>(true) };
            ui::ui_focus_scope_t& scope{ root->emplace_child<ui::ui_focus_scope_t>() };
            scope.set_visibility(ui::ui_widget_visibility_t::collapsed);
            scope.set_focus_on_show_policy(ui::ui_focus_on_show_policy_t::last);

            navigation_probe_widget_t& scoped_a{ scope.emplace_child<navigation_probe_widget_t>(true) };
            navigation_probe_widget_t& scoped_b{ scope.emplace_child<navigation_probe_widget_t>(true) };

            CARROT_TEST_REQUIRE(module.set_focus(&outside));
            scope.set_visibility(ui::ui_widget_visibility_t::visible);
            module.update(1.f / 60.f);
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &scoped_a);

            CARROT_TEST_REQUIRE(module.focus_next());
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &scoped_b);

            scope.set_visibility(ui::ui_widget_visibility_t::collapsed);
            module.update(1.f / 60.f);
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &outside);

            scope.set_visibility(ui::ui_widget_visibility_t::visible);
            module.update(1.f / 60.f);
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &scoped_b);
        }

        void test_ui_focus_scope_show_policy_explicit_targets_requested_widget()
        {
            ui::ui_module_t module;
            module.init();

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            ui::ui_focus_scope_t& scope{ root->emplace_child<ui::ui_focus_scope_t>() };
            scope.set_visibility(ui::ui_widget_visibility_t::collapsed);
            scope.set_focus_on_show_policy(ui::ui_focus_on_show_policy_t::explicit_target);

            navigation_probe_widget_t& scoped_a{ scope.emplace_child<navigation_probe_widget_t>(true) };
            navigation_probe_widget_t& scoped_b{ scope.emplace_child<navigation_probe_widget_t>(true) };
            scope.set_explicit_focus_target(&scoped_b);

            scope.set_visibility(ui::ui_widget_visibility_t::visible);
            module.update(1.f / 60.f);
            CARROT_TEST_REQUIRE(module.get_focused_widget() == &scoped_b);

            (void)scoped_a;
        }

        void test_ui_module_feedback_hooks_emit_focus_accept_cancel_events()
        {
            ui::ui_module_t module;
            module.init();

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            navigation_probe_widget_t& first{ root->emplace_child<navigation_probe_widget_t>(true) };
            root->emplace_child<navigation_probe_widget_t>(true);
            CARROT_TEST_REQUIRE(module.set_focus(&first));

            std::vector<ui::ui_feedback_event_t> events;
            module.set_feedback_callback([&events](const ui::ui_feedback_event_t event) { events.push_back(event); });

            CARROT_TEST_REQUIRE(module.handle_navigation_action(ui::ui_navigation_action_t::move_down));
            CARROT_TEST_REQUIRE(module.handle_navigation_action(ui::ui_navigation_action_t::accept));
            CARROT_TEST_REQUIRE(module.handle_navigation_action(ui::ui_navigation_action_t::cancel));

            CARROT_TEST_REQUIRE(events.size() == 3u);
            CARROT_TEST_REQUIRE(events[0] == ui::ui_feedback_event_t::focus_move);
            CARROT_TEST_REQUIRE(events[1] == ui::ui_feedback_event_t::accept);
            CARROT_TEST_REQUIRE(events[2] == ui::ui_feedback_event_t::cancel);
        }

        void test_ui_module_tracks_navigation_action_debug_stream()
        {
            ui::ui_module_t module;
            module.init();

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            navigation_probe_widget_t& widget{ root->emplace_child<navigation_probe_widget_t>(true) };
            CARROT_TEST_REQUIRE(module.set_focus(&widget));

            CARROT_TEST_REQUIRE(module.handle_navigation_action(ui::ui_navigation_action_t::accept));
            CARROT_TEST_REQUIRE(!module.handle_navigation_action("ui_not_real"));

            const std::vector<std::string>& stream{ module.get_debug_navigation_events() };
            CARROT_TEST_REQUIRE(!stream.empty());
            CARROT_TEST_REQUIRE(stream.back().find("unknown:ui_not_real") != std::string::npos);
        }

        void test_ui_module_dispatch_respects_input_ownership_modes()
        {
            ui::ui_module_t module;
            module.init();

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            navigation_probe_widget_t& widget{ root->emplace_child<navigation_probe_widget_t>(true) };
            CARROT_TEST_REQUIRE(module.set_focus(&widget));

            module.set_input_ownership_mode(ui::ui_input_ownership_mode_t::ui_priority);
            CARROT_TEST_REQUIRE(module.dispatch_navigation_action("ui_accept"));

            module.set_input_ownership_mode(ui::ui_input_ownership_mode_t::passthrough);
            CARROT_TEST_REQUIRE(!module.dispatch_navigation_action("ui_accept"));

            module.clear_focus();
            module.set_input_ownership_mode(ui::ui_input_ownership_mode_t::ui_exclusive);
            CARROT_TEST_REQUIRE(module.dispatch_navigation_action("ui_accept"));
            CARROT_TEST_REQUIRE(!module.dispatch_navigation_action("ui_not_real"));
        }

        void test_ui_focus_scope_trap_enforces_exclusive_consumption_by_default()
        {
            ui::ui_module_t module;
            module.init();
            module.set_input_ownership_mode(ui::ui_input_ownership_mode_t::passthrough);

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            ui::ui_focus_scope_t& scope{ root->emplace_child<ui::ui_focus_scope_t>() };
            scope.set_focus_trap_enabled(true);
            scope.emplace_child<navigation_probe_widget_t>(true);

            module.update(1.f / 60.f);
            CARROT_TEST_REQUIRE(module.get_effective_input_ownership_mode() == ui::ui_input_ownership_mode_t::ui_exclusive);
            CARROT_TEST_REQUIRE(module.dispatch_navigation_action("ui_accept"));
        }

        void test_ui_focus_scope_input_override_can_disable_consumption()
        {
            ui::ui_module_t module;
            module.init();
            module.set_input_ownership_mode(ui::ui_input_ownership_mode_t::ui_priority);

            ui::ui_root_widget_t* root{ module.get_root() };
            CARROT_TEST_REQUIRE(root != nullptr);

            ui::ui_focus_scope_t& scope{ root->emplace_child<ui::ui_focus_scope_t>() };
            scope.set_focus_trap_enabled(true);
            scope.set_input_ownership_override(ui::ui_input_ownership_mode_t::passthrough);
            scope.emplace_child<navigation_probe_widget_t>(true);

            module.update(1.f / 60.f);
            CARROT_TEST_REQUIRE(module.get_effective_input_ownership_mode() == ui::ui_input_ownership_mode_t::passthrough);
            CARROT_TEST_REQUIRE(!module.dispatch_navigation_action("ui_accept"));
        }
    } // namespace

    void register_ui_navigation_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("ui navigation focus traversal skips non-focusable widgets",
                           test_ui_module_focus_traversal_skips_non_focusable_widgets);
        tests.emplace_back("ui navigation focus traversal loops by default",
                           test_ui_module_focus_traversal_loops_by_default);
        tests.emplace_back("ui navigation uses explicit directional targets when present",
                           test_ui_module_navigation_respects_explicit_directional_targets);
        tests.emplace_back("ui navigation accept/cancel and action-name routing works",
                           test_ui_module_accept_cancel_and_action_name_routing);
        tests.emplace_back("ui navigation clears focus when focused widget leaves tree",
                           test_ui_module_clears_focus_when_focused_widget_leaves_tree);
        tests.emplace_back("ui button emits focus and action callbacks",
                           test_ui_button_focus_accept_cancel_callbacks_fire);
        tests.emplace_back("ui focus scope traps focus and restores on hide",
                           test_ui_focus_scope_traps_focus_and_restores_on_hide);
        tests.emplace_back("ui focus scope show policy last restores previous focus",
                           test_ui_focus_scope_show_policy_last_restores_previous_focus);
        tests.emplace_back("ui focus scope show policy explicit picks explicit target",
                           test_ui_focus_scope_show_policy_explicit_targets_requested_widget);
        tests.emplace_back("ui module emits feedback hook events for focus/accept/cancel",
                           test_ui_module_feedback_hooks_emit_focus_accept_cancel_events);
        tests.emplace_back("ui module records navigation debug stream entries",
                           test_ui_module_tracks_navigation_action_debug_stream);
        tests.emplace_back("ui module dispatch respects input ownership modes",
                           test_ui_module_dispatch_respects_input_ownership_modes);
        tests.emplace_back("ui focus scope trap defaults to exclusive input consumption",
                           test_ui_focus_scope_trap_enforces_exclusive_consumption_by_default);
        tests.emplace_back("ui focus scope can override input consumption policy",
                           test_ui_focus_scope_input_override_can_disable_consumption);
    }
} // namespace carrot::tests
