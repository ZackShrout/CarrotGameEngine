//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SandboxGame.h"

#include "GameplayState.h"
#include "SandboxInputConfig.h"

namespace sandbox {
    namespace {
        constexpr std::string_view k_dialogue_lorem_ipsum{
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus finibus, nibh sed "
            "tristique fermentum, augue urna malesuada nibh, vel hendrerit lacus libero sit amet "
            "erat. Integer facilisis nisl non sem placerat, quis efficitur ipsum vulputate."
        };

        [[nodiscard]] const char* ui_feedback_event_to_string(const carrot::ui::ui_feedback_event_t event) noexcept
        {
            switch (event)
            {
                case carrot::ui::ui_feedback_event_t::focus_move: return "focus_move";
                case carrot::ui::ui_feedback_event_t::accept: return "accept";
                case carrot::ui::ui_feedback_event_t::cancel: return "cancel";
                default: return "unknown";
            }
        }
    } // namespace

    sandbox_game_t::sandbox_game_t(carrot::core::game_context_t& game) noexcept
        : carrot::core::game_runtime_t(game)
    {
    }

    void sandbox_game_t::configure_fallback_input_actions()
    {
        _input.clear();

        _input.bind(std::string{ k_action_move_up }, carrot::input::key_code::w);
        _input.bind(std::string{ k_action_move_up }, carrot::input::key_code::up);
        _input.bind_gamepad_button(std::string{ k_action_move_up }, carrot::input::gamepad_button_t::dpad_up);
        _input.bind_gamepad_axis(std::string{ k_action_move_up },
                                 carrot::input::gamepad_axis_t::left_y,
                                 carrot::input::gamepad_axis_direction_t::negative);
        _input.bind(std::string{ k_action_move_down }, carrot::input::key_code::s);
        _input.bind(std::string{ k_action_move_down }, carrot::input::key_code::down);
        _input.bind_gamepad_button(std::string{ k_action_move_down }, carrot::input::gamepad_button_t::dpad_down);
        _input.bind_gamepad_axis(std::string{ k_action_move_down },
                                 carrot::input::gamepad_axis_t::left_y,
                                 carrot::input::gamepad_axis_direction_t::positive);
        _input.bind(std::string{ k_action_move_left }, carrot::input::key_code::a);
        _input.bind(std::string{ k_action_move_left }, carrot::input::key_code::left);
        _input.bind_gamepad_button(std::string{ k_action_move_left }, carrot::input::gamepad_button_t::dpad_left);
        _input.bind_gamepad_axis(std::string{ k_action_move_left },
                                 carrot::input::gamepad_axis_t::left_x,
                                 carrot::input::gamepad_axis_direction_t::negative);
        _input.bind(std::string{ k_action_move_right }, carrot::input::key_code::d);
        _input.bind(std::string{ k_action_move_right }, carrot::input::key_code::right);
        _input.bind_gamepad_button(std::string{ k_action_move_right }, carrot::input::gamepad_button_t::dpad_right);
        _input.bind_gamepad_axis(std::string{ k_action_move_right },
                                 carrot::input::gamepad_axis_t::left_x,
                                 carrot::input::gamepad_axis_direction_t::positive);

        _input.bind(std::string{ k_action_interact }, carrot::input::key_code::e);
        _input.bind_gamepad_button(std::string{ k_action_interact }, carrot::input::gamepad_button_t::south);
        _input.bind(std::string{ k_action_quit }, carrot::input::key_code::escape);
        _input.bind(std::string{ k_action_toggle_fullscreen }, carrot::input::key_code::f11);
        _input.bind(std::string{ k_action_toggle_fullscreen },
                    carrot::input::key_code::enter,
                    static_cast<uint8_t>(carrot::input::modifier::alt));
        _input.bind(std::string{ k_action_toggle_map_collision_debug }, carrot::input::key_code::f2);
        _input.bind(std::string{ k_action_toggle_object_collision_debug }, carrot::input::key_code::f3);
        _input.bind(std::string{ k_action_ui_up }, carrot::input::key_code::i);
        _input.bind(std::string{ k_action_ui_down }, carrot::input::key_code::k);
        _input.bind(std::string{ k_action_ui_left }, carrot::input::key_code::j);
        _input.bind(std::string{ k_action_ui_right }, carrot::input::key_code::l);
        _input.bind(std::string{ k_action_ui_accept }, carrot::input::key_code::o);
        _input.bind(std::string{ k_action_ui_cancel }, carrot::input::key_code::p);
    }

    void sandbox_game_t::configure_default_input_actions()
    {
        if (_input.load_bindings_from_file(game().assets.vfs(), k_input_bindings_config_path))
        {
            LOG_CORE_INFO("Loaded input bindings from '{}'", k_input_bindings_config_path);
            return;
        }

        LOG_CORE_WARN("Falling back to built-in sandbox input bindings");
        configure_fallback_input_actions();
    }

    void sandbox_game_t::bootstrap_runtime_ui() noexcept
    {
        if (carrot::ui::ui_module_t* ui_module{ carrot::ui::ui_service_t::try_get() })
        {
            ui_module->set_feedback_callback([](const carrot::ui::ui_feedback_event_t event)
            {
                LOG_UI_INFO("UI feedback: {}", ui_feedback_event_to_string(event));
            });

            carrot::ui::ui_root_widget_t* root{ ui_module->get_root() };
            if (!root)
                return;

            root->remove_all_children();
            carrot::ui::ui_stack_t& screen{ root->emplace_child<carrot::ui::ui_stack_t>(carrot::ui::ui_stack_orientation_t::vertical) };
            screen.set_padding({ .left = 24.f, .top = 24.f, .right = 32.f, .bottom = 24.f });
            screen.set_spacing(24.f);
            screen.set_cross_alignment(carrot::ui::ui_stack_cross_alignment_t::stretch);

            carrot::ui::ui_stack_t& top_row{
                screen.emplace_child<carrot::ui::ui_stack_t>(carrot::ui::ui_stack_orientation_t::horizontal)
            };
            top_row.set_cross_alignment(carrot::ui::ui_stack_cross_alignment_t::start);
            top_row.set_main_axis_size_rule(carrot::ui::ui_main_axis_size_rule_t::desired);

            carrot::ui::ui_panel_t& top_spacer{ top_row.emplace_child<carrot::ui::ui_panel_t>() };
            top_spacer.set_visibility(carrot::ui::ui_widget_visibility_t::hidden);
            top_spacer.set_main_axis_size_rule(carrot::ui::ui_main_axis_size_rule_t::flex);

            carrot::ui::ui_stack_t& menu{ top_row.emplace_child<carrot::ui::ui_stack_t>(carrot::ui::ui_stack_orientation_t::vertical) };
            menu.set_spacing(10.f);
            menu.set_cross_alignment(carrot::ui::ui_stack_cross_alignment_t::end);

            auto& title{ menu.emplace_child<carrot::ui::ui_label_t>("Runtime Menu") };
            title.set_font_size(32.0f);
            title.set_color(0xFFFFD86Bu);
            title.set_horizontal_alignment(carrot::ui::ui_label_horizontal_alignment_t::end);
            auto& item_a{ menu.emplace_child<carrot::ui::ui_button_t>("Start Game") };
            auto& item_b{ menu.emplace_child<carrot::ui::ui_button_t>("Options") };
            auto& item_c{ menu.emplace_child<carrot::ui::ui_button_t>("Exit") };
            item_a.set_navigation_target(carrot::ui::ui_navigation_direction_t::down, &item_b);
            item_b.set_navigation_target(carrot::ui::ui_navigation_direction_t::up, &item_a);
            item_b.set_navigation_target(carrot::ui::ui_navigation_direction_t::down, &item_c);
            item_c.set_navigation_target(carrot::ui::ui_navigation_direction_t::up, &item_b);

            carrot::ui::ui_panel_t& middle_spacer{ screen.emplace_child<carrot::ui::ui_panel_t>() };
            middle_spacer.set_visibility(carrot::ui::ui_widget_visibility_t::hidden);
            middle_spacer.set_main_axis_size_rule(carrot::ui::ui_main_axis_size_rule_t::flex);

            carrot::ui::ui_panel_t& dialogue_panel{ screen.emplace_child<carrot::ui::ui_panel_t>() };
            dialogue_panel.set_desired_size({ 720.f, 184.f });
            dialogue_panel.set_min_size({ 360.f, 160.f });

            carrot::ui::ui_panel_style_t dialogue_style{ };
            dialogue_style.fill_color = 0xE61C2333u;
            dialogue_style.border_color = 0xFF8DA4C2u;
            dialogue_style.border_thickness = 3.0f;
            dialogue_style.padding = { 22.f, 18.f, 22.f, 18.f };
            dialogue_panel.set_style(dialogue_style);

            carrot::ui::ui_stack_t& dialogue_content{
                dialogue_panel.emplace_child<carrot::ui::ui_stack_t>(carrot::ui::ui_stack_orientation_t::vertical)
            };
            dialogue_content.set_spacing(10.f);
            dialogue_content.set_cross_alignment(carrot::ui::ui_stack_cross_alignment_t::stretch);

            auto& speaker{ dialogue_content.emplace_child<carrot::ui::ui_label_t>("Archivist") };
            speaker.set_font_size(20.0f);
            speaker.set_color(0xFFFFD86Bu);

            auto& body{ dialogue_content.emplace_child<carrot::ui::ui_label_t>(std::string{ k_dialogue_lorem_ipsum }) };
            body.set_font_size(22.0f);
            body.set_color(0xFFF4F0E8u);
            body.set_wrap_width(676.0f);
            body.set_main_axis_size_rule(carrot::ui::ui_main_axis_size_rule_t::flex);

            (void)ui_module->focus_first();
        }
    }

    void sandbox_game_t::start()
    {
        configure_default_input_actions();
        bootstrap_runtime_ui();
        set_active_state(std::make_unique<gameplay_state_t>(*this, _input));
    }

    void sandbox_game_t::tick(const float delta_time)
    {
        carrot::core::game_runtime_t::tick(delta_time);

        if (_input.was_just_pressed(k_action_quit))
            quit_application();
    }

    void sandbox_game_t::on_key(const carrot::events::key_event_t& e)
    {
        const bool toggle_fullscreen{ _input.handle_key_event(e, k_input_profile, false) };
        carrot::core::game_runtime_t::on_key(e);

        if (toggle_fullscreen)
            set_fullscreen(!is_fullscreen());
    }

    void sandbox_game_t::on_mouse_moved(const carrot::events::mouse_moved_event_t& e)
    {
        carrot::core::game_runtime_t::on_mouse_moved(e);

        static int move_counter = 0;
        if (++move_counter % 5 == 0)
        {
            LOG_CORE_TRACE("Mouse: {:.0f}, {:.0f} (delta {:.1f}, {:.1f})",
                           e._pos.x,
                           e._pos.y,
                           e._delta.x,
                           e._delta.y);
        }
    }

    void sandbox_game_t::on_mouse_button(const carrot::events::mouse_button_event_t& e)
    {
        carrot::core::game_runtime_t::on_mouse_button(e);

        if (e._action == carrot::events::key_action::press)
            LOG_CORE_INFO("Mouse Button {} ({}) pressed",
                          carrot::input::mouse_button_to_string(e._button),
                          static_cast<uint32_t>(e._button));
        else if (e._action == carrot::events::key_action::release)
            LOG_CORE_INFO("Mouse Button {} ({}) released",
                          carrot::input::mouse_button_to_string(e._button),
                          static_cast<uint32_t>(e._button));
    }

    void sandbox_game_t::on_mouse_scrolled(const carrot::events::mouse_scrolled_event_t& e)
    {
        carrot::core::game_runtime_t::on_mouse_scrolled(e);

        LOG_CORE_INFO("Mouse wheel scrolled: {} {}",
                      static_cast<int32_t>(e._delta.x),
                      static_cast<int32_t>(e._delta.y));
    }
} // namespace sandbox
