//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "EditorApp.h"

namespace carrot::editor {
    namespace {
        [[nodiscard]] constexpr uint32_t make_abgr(const uint8_t r,
                                                   const uint8_t g,
                                                   const uint8_t b,
                                                   const uint8_t a = 0xFFu) noexcept
        {
            return (static_cast<uint32_t>(a) << 24u) |
                   (static_cast<uint32_t>(b) << 16u) |
                   (static_cast<uint32_t>(g) << 8u) |
                   static_cast<uint32_t>(r);
        }

        constexpr uint32_t color_bg{ make_abgr(0x03u, 0x03u, 0x04u) };
        constexpr uint32_t color_panel{ make_abgr(0x09u, 0x09u, 0x0Bu) };
        constexpr uint32_t color_panel_alt{ make_abgr(0x07u, 0x08u, 0x09u) };
        constexpr uint32_t color_border{ make_abgr(0x1Cu, 0x1Cu, 0x1Fu) };
        constexpr uint32_t color_text_primary{ make_abgr(0xFFu, 0xFFu, 0xFFu) };
        constexpr uint32_t color_text_secondary{ make_abgr(0xD2u, 0xD7u, 0xDEu) };
        constexpr uint32_t color_text_muted{ make_abgr(0x8Cu, 0x94u, 0x9Fu) };
        constexpr uint32_t color_accent{ make_abgr(0xC6u, 0x74u, 0x3Fu) };
        constexpr uint32_t color_accent_soft{ make_abgr(0x4Eu, 0x2Fu, 0x20u) };
        constexpr uint32_t color_accent_glow{ make_abgr(0xE6u, 0x9Du, 0x60u) };
        constexpr uint32_t color_selection_fill{ make_abgr(0x18u, 0x12u, 0x0Eu) };
        constexpr uint32_t color_selection_border{ make_abgr(0xC6u, 0x74u, 0x3Fu) };
        constexpr uint32_t color_button_fill{ make_abgr(0x0Au, 0x0Au, 0x0Cu) };
        constexpr uint32_t color_button_border{ make_abgr(0x21u, 0x20u, 0x23u) };
        constexpr uint32_t color_button_fill_disabled{ make_abgr(0x04u, 0x04u, 0x06u) };

        [[nodiscard]] ui::ui_panel_style_t background_panel_style() noexcept
        {
            ui::ui_panel_style_t style;
            style.fill_color = color_bg;
            style.border_color = color_bg;
            style.border_thickness = 0.f;
            style.padding = { 0.f, 0.f, 0.f, 0.f };
            return style;
        }

        [[nodiscard]] ui::ui_panel_style_t chrome_panel_style(const bool alternate = false) noexcept
        {
            ui::ui_panel_style_t style;
            style.fill_color = alternate ? color_panel_alt : color_panel;
            style.border_color = color_border;
            style.border_thickness = 1.f;
            style.padding = { 20.f, 20.f, 20.f, 20.f };
            return style;
        }

        [[nodiscard]] ui::ui_button_style_t editor_button_style() noexcept
        {
            ui::ui_button_style_t style;
            style.normal.fill_color = color_button_fill;
            style.normal.border_color = color_button_border;
            style.focused.fill_color = color_selection_fill;
            style.focused.border_color = color_selection_border;
            style.disabled.fill_color = color_button_fill_disabled;
            style.disabled.border_color = color_border;
            style.label_color = color_text_secondary;
            style.focused_label_color = color_text_primary;
            style.disabled_label_color = color_text_muted;
            style.border_thickness = 1.5f;
            style.content_padding = { 14.f, 7.f, 14.f, 7.f };
            return style;
        }

        [[nodiscard]] ui::ui_button_style_t primary_action_button_style() noexcept
        {
            ui::ui_button_style_t style = editor_button_style();
            style.normal.fill_color = make_abgr(0x12u, 0x12u, 0x14u);
            style.normal.border_color = make_abgr(0x24u, 0x23u, 0x27u);
            style.focused.fill_color = color_accent_soft;
            style.focused.border_color = color_accent_glow;
            style.disabled.fill_color = color_button_fill_disabled;
            style.disabled.border_color = color_border;
            style.label_color = color_text_primary;
            style.focused_label_color = make_abgr(0xFFu, 0xF6u, 0xECu);
            return style;
        }

        [[nodiscard]] ui::ui_asset_preview_style_t editor_preview_style() noexcept
        {
            ui::ui_asset_preview_style_t style;
            style.background_color = make_abgr(0x08u, 0x08u, 0x09u);
            style.frame_color = color_border;
            style.image_tint = 0xFFFFFFFFu;
            style.border_thickness = 1.5f;
            style.padding = 18.f;
            return style;
        }

        [[nodiscard]] std::string_view asset_kind_short_label(const assets::asset_kind_t kind) noexcept
        {
            switch (kind)
            {
                case assets::asset_kind_t::font: return "F";
                case assets::asset_kind_t::texture: return "T";
                case assets::asset_kind_t::sprite: return "S";
                case assets::asset_kind_t::audio: return "A";
                case assets::asset_kind_t::tilemap: return "M";
                case assets::asset_kind_t::scene: return "C";
                default: return "?";
            }
        }

        [[nodiscard]] std::string format_asset_list_label(const assets::asset_iteration_status_t& status,
                                                          const bool selected)
        {
            std::string label;
            label.reserve(status.logical_id.size() + 40u);

            label += selected ? "> " : "  ";
            label += "[";
            label += asset_kind_short_label(status.kind);
            label += "] ";
            label += status.logical_id;

            if (status.has_last_attempt && status.last_result == assets::asset_iteration_result_t::failed)
            {
                label += "  !";
            }
            else if (status.loaded_in_runtime_cache)
            {
                label += "  *";
            }

            return label;
        }

        [[nodiscard]] std::string format_asset_details(const assets::asset_iteration_status_t& status,
                                                       const bool has_active_scene)
        {
            std::string text;
            text.reserve(512u);

            const auto append_line = [&text](std::string_view key, const std::string_view value)
            {
                text += key;
                text += ": ";
                text += value;
                text += '\n';
            };

            const assets::asset_runtime_refresh_action_t action{
                assets::recommended_runtime_refresh_action(status, has_active_scene)
            };
            append_line("Kind", assets::to_string(status.kind));
            append_line("Dependency Shape", assets::to_string(status.dependency_shape));
            append_line("Watch Mode", assets::to_string(status.watch_mode));
            append_line("Last Watch Change", assets::to_string(status.last_watch_change));
            append_line("Reload Policy", assets::to_string(status.reload_policy));
            append_line("Recommended Runtime Action", assets::to_string(action));
            append_line("Action Reason", assets::describe_runtime_refresh_action_reason(status, has_active_scene));
            append_line("Source", status.source_uri);
            append_line("Manifest", status.manifest_uri.empty() ? std::string_view{ "<none>" } : std::string_view{ status.manifest_uri });
            append_line("Dependency Summary",
                        status.dependency_summary.empty() ? std::string_view{ "<none>" } : std::string_view{ status.dependency_summary });
            append_line("Watch Detail", assets::describe_watch_change(status.last_watch_change));
            append_line("Cached In Runtime", status.loaded_in_runtime_cache ? std::string_view{ "yes" } : std::string_view{ "no" });
            append_line("Last Request Origin", assets::to_string(status.last_refresh_request_origin));
            append_line("Last Requested Action", assets::to_string(status.last_requested_action));

            if (!status.has_last_attempt)
            {
                append_line("Last Attempt", "never");
                append_line("Attempt Summary", assets::describe_last_attempt_summary(status));
                return text;
            }

            append_line("Last Attempt", assets::to_string(status.last_result));
            append_line("Attempt Summary", assets::describe_last_attempt_summary(status));
            append_line("Load Origin", assets::to_string(status.last_load_origin));
            append_line("Cooked Artifact", assets::to_string(status.last_cooked_artifact_state));
            append_line("Invalidation Reason", assets::to_string(status.last_invalidation_reason));
            append_line("Invalidation Detail", assets::describe_invalidation_reason(status.last_invalidation_reason));
            append_line("Last Error", status.last_error.empty() ? std::string_view{ "<none>" } : std::string_view{ status.last_error });

            return text;
        }

        [[nodiscard]] uint32_t status_color(const assets::asset_iteration_status_t& status) noexcept
        {
            if (!status.has_last_attempt)
                return color_text_secondary;

            return status.last_result == assets::asset_iteration_result_t::success
                ? color_accent_glow
                : make_abgr(0xD9u, 0x8Bu, 0x73u);
        }

        [[nodiscard]] assets::asset_runtime_refresh_action_t selected_action(
            const assets::asset_iteration_status_t& status,
            const bool has_active_scene) noexcept
        {
            return assets::recommended_runtime_refresh_action(status, has_active_scene);
        }

        [[nodiscard]] std::string_view action_button_label(const assets::asset_runtime_refresh_action_t action) noexcept
        {
            switch (action)
            {
                case assets::asset_runtime_refresh_action_t::reload_now: return "Reload Selected Asset";
                case assets::asset_runtime_refresh_action_t::reload_on_next_use: return "Queue Refresh On Next Use";
                case assets::asset_runtime_refresh_action_t::manual_refresh: return "Refresh Selected Asset";
                case assets::asset_runtime_refresh_action_t::rebuild_current_scene: return "Rebuild Current Scene";
                case assets::asset_runtime_refresh_action_t::restart_runtime: return "Restart Runtime Required";
                case assets::asset_runtime_refresh_action_t::none:
                default: return "Reload Selected Asset";
            }
        }

        [[nodiscard]] bool can_trigger_action(const assets::asset_runtime_refresh_action_t action,
                                             const bool can_rebuild_current_scene) noexcept
        {
            switch (action)
            {
                case assets::asset_runtime_refresh_action_t::reload_now:
                case assets::asset_runtime_refresh_action_t::reload_on_next_use:
                case assets::asset_runtime_refresh_action_t::manual_refresh:
                    return true;
                case assets::asset_runtime_refresh_action_t::rebuild_current_scene:
                    return can_rebuild_current_scene;
                case assets::asset_runtime_refresh_action_t::restart_runtime:
                case assets::asset_runtime_refresh_action_t::none:
                default:
                    return false;
            }
        }

        [[nodiscard]] std::string format_vec2(const chlm::float2 value)
        {
            return std::format("{:.2f}, {:.2f}", value.x, value.y);
        }

        [[nodiscard]] std::string format_color4(const chlm::float4 value)
        {
            return std::format("{:.2f}, {:.2f}, {:.2f}, {:.2f}", value.x, value.y, value.z, value.w);
        }

        [[nodiscard]] std::string format_scene_summary(const scene::scene_runtime_summary_t& summary)
        {
            std::string text;
            const auto append_line = [&text](std::string_view key, const std::string& value)
            {
                text += std::format("{}: {}\n", key, value);
            };

            append_line("Scene", summary.snapshot.active_scene_id.empty() ? "<none>" : std::string{ summary.snapshot.active_scene_id });
            append_line("State", std::string{ scene::to_string(summary.snapshot.runtime_state) });
            append_line("Phase", std::string{ scene::to_string(summary.snapshot.transition_phase) });
            append_line("Spawn", summary.snapshot.active_spawn_marker.empty() ? "<none>" : std::string{ summary.snapshot.active_spawn_marker });
            if (summary.diagnostics.visible || summary.diagnostics.outcome != scene::scene_change_outcome_t::none)
            {
                append_line("Change",
                            std::format("{} / {}",
                                        scene::to_string(summary.diagnostics.request_kind),
                                        scene::to_string(summary.diagnostics.outcome)));
                append_line("Target",
                            summary.diagnostics.target_scene_id.empty()
                                ? "<none>"
                                : std::format("{} @ {}",
                                              summary.diagnostics.target_scene_id,
                                              summary.diagnostics.target_spawn_marker.empty()
                                                  ? "<none>"
                                                  : summary.diagnostics.target_spawn_marker));
                append_line("Refresh",
                            std::format("overlay {}, preserved active scene {}, progress {:.0f}%",
                                        scene::to_string(summary.diagnostics.overlay_style),
                                        summary.diagnostics.preserved_active_scene ? "yes" : "no",
                                        std::round(std::clamp(summary.diagnostics.transition_progress, 0.f, 1.f) *
                                                   100.f)));
                if (summary.diagnostics.has_structural_refresh_context())
                {
                    append_line("Structural Refresh",
                                std::format("{} {}",
                                            assets::to_string(summary.diagnostics.structural_refresh_asset_kind),
                                            summary.diagnostics.structural_refresh_asset_logical_id));
                    append_line("Refresh Reason", summary.diagnostics.structural_refresh_reason);
                }
            }
            append_line("Camera", std::format("zoom {:.2f} at {}", summary.active_camera.zoom, format_vec2(summary.camera_center_world)));
            append_line("World", std::format("{} objects, {} triggers, {} colliders, {} static, {} lights, {} vis regions",
                                             summary.world_object_count,
                                             summary.trigger_count,
                                             summary.object_collider_count,
                                             summary.static_collider_count,
                                             summary.point_light_count,
                                             summary.visibility_region_count));
            if (summary.has_player_object())
                append_line("Player", std::format("{} (#{} )", summary.player_object_name, summary.player_object_id));
            if (summary.has_spawn_object())
                append_line("Spawn Object", std::format("{} (#{} )", summary.spawn_object_name, summary.spawn_object_id));
            return text;
        }

        [[nodiscard]] std::string format_systems_summary(const scene::scene_runtime_systems_summary_t& summary)
        {
            std::string text;
            const auto append_line = [&text](std::string_view key, const std::string& value)
            {
                text += std::format("{}: {}\n", key, value);
            };

            append_line("Lighting", std::format("{} point light(s), ambient {}", summary.lighting.point_light_count, format_color4(summary.lighting.ambient_color)));
            append_line("Collision", std::format("{} static, map={}, object={}, trigger={}",
                                                 summary.collision.static_collider_count,
                                                 summary.collision.show_map_collision ? "on" : "off",
                                                 summary.collision.show_object_colliders ? "on" : "off",
                                                 summary.collision.show_trigger_volumes ? "on" : "off"));
            append_line("Layering", std::format("frame {}, vis regions {}, active tags {}",
                                                summary.layering.frame_index,
                                                summary.layering.visibility_region_count,
                                                summary.layering.active_visibility_tags.size()));
            if (summary.player_controller.bound)
            {
                append_line("Player Ctrl", std::format("{} facing {}, speed {:.2f}",
                                                       summary.player_controller.controlled_object_name.empty()
                                                           ? "<unbound>"
                                                           : summary.player_controller.controlled_object_name,
                                                       summary.player_controller.facing_direction,
                                                       summary.player_controller.move_speed));
            }
            else
            {
                append_line("Player Ctrl", "unbound");
            }

            if (summary.interaction_controller.bound)
            {
                append_line("Interact Ctrl", std::format("actor {}, radius {:.2f}, candidate {}",
                                                         summary.interaction_controller.actor_object_name.empty()
                                                             ? "<none>"
                                                             : summary.interaction_controller.actor_object_name,
                                                         summary.interaction_controller.interaction_radius,
                                                         summary.interaction_controller.has_candidate
                                                             ? summary.interaction_controller.candidate_object_name
                                                             : "<none>"));
            }
            else
            {
                append_line("Interact Ctrl", "unbound");
            }

            return text;
        }

        [[nodiscard]] std::string format_runtime_object_list_label(const scene::scene_runtime_object_summary_t& summary,
                                                                   const bool selected)
        {
            std::string label;
            label.reserve(summary.name.size() + summary.type.size() + 32u);
            label += selected ? "> " : "  ";
            label += summary.name.empty() ? "<unnamed>" : summary.name;
            if (!summary.type.empty())
                label += std::format(" [{}]", summary.type);
            label += std::format(" #{}", summary.id);
            return label;
        }

        [[nodiscard]] std::string format_runtime_object_details(const scene::scene_runtime_object_summary_t& summary)
        {
            std::string text;
            const auto append_line = [&text](std::string_view key, const std::string& value)
            {
                text += std::format("{}: {}\n", key, value);
            };

            append_line("Name", summary.name.empty() ? "<unnamed>" : summary.name);
            append_line("Type", summary.type.empty() ? "<none>" : summary.type);
            append_line("Id", std::to_string(summary.id));
            append_line("Properties", std::to_string(summary.property_count));

            if (summary.has_source)
            {
                append_line("Source", std::format("{} / {} / {}",
                                                  summary.source.tilemap_logical_id,
                                                  summary.source.layer_name,
                                                  summary.source.object_name.empty() ? "<unnamed>" : summary.source.object_name));
            }

            if (summary.has_transform)
                append_line("Transform", std::format("pos {} scale {} rot {:.2f}",
                                                     format_vec2(summary.transform.position),
                                                     format_vec2(summary.transform.scale),
                                                     summary.transform.rotation));
            if (summary.has_collision)
                append_line("Collision", std::format("half {} offset {}",
                                                     format_vec2(summary.collision.half_extents),
                                                     format_vec2(summary.collision.offset)));
            if (summary.has_trigger)
                append_line("Trigger", std::format("{} / {}", summary.trigger.trigger_id, summary.trigger.trigger_kind));
            if (summary.has_sprite)
                append_line("Sprite", std::format("tex {} frame {}",
                                                  summary.sprite.texture_id.empty() ? "<none>" : summary.sprite.texture_id,
                                                  summary.sprite.frame_name.empty() ? "<none>" : summary.sprite.frame_name));
            if (summary.has_sprite_animator)
                append_line("Animator", std::format("{} / {}",
                                                    summary.sprite_animator.current_animation_name.empty()
                                                        ? "<idle>"
                                                        : summary.sprite_animator.current_animation_name,
                                                    summary.sprite_animator.current_frame_name.empty()
                                                        ? "<none>"
                                                        : summary.sprite_animator.current_frame_name));
            if (summary.has_tilemap)
                append_line("Tilemap", summary.tilemap.tilemap_logical_id.empty() ? "<none>" : summary.tilemap.tilemap_logical_id);
            if (summary.has_tile_object)
                append_line("Tile Object", std::format("{} gid {}", summary.tile_object.tilemap_logical_id, summary.tile_object.gid));
            if (summary.has_visibility_region)
                append_line("Visibility", std::format("{} ({})",
                                                      summary.visibility_region.tag,
                                                      format_vec2(summary.visibility_region.size_world)));
            if (summary.has_interaction)
            {
                append_line("Interaction", std::string{ scene::to_string(summary.interaction.kind) });
                if (!summary.interaction.message_id.empty())
                    append_line("Message", summary.interaction.message_id);
                if (!summary.interaction.target_scene.empty() || !summary.interaction.target_marker.empty())
                    append_line("Door", std::format("{} -> {}",
                                                    summary.interaction.target_scene.empty() ? summary.interaction.target_map
                                                                                            : summary.interaction.target_scene,
                                                    summary.interaction.target_marker));
                if (!summary.interaction.loot_table.empty())
                    append_line("Loot", summary.interaction.loot_table);
                if (!summary.interaction.trigger_id.empty())
                    append_line("Trigger Id", summary.interaction.trigger_id);
            }

            return text;
        }
    } // namespace

    void editor_app_t::describe_boot_prewarm(core::boot_prewarm_plan_t& plan) const
    {
        plan.font_ids.emplace_back("font.engine.roboto_regular");
    }

    void editor_app_t::start(core::game_context_t& game)
    {
        _game = &game;
        build_editor_ui();
        rebuild_asset_list();
        refresh_selected_asset_details();

        // Temporary milestone harness: boot the known sandbox town so the new
        // runtime inspection panels can be validated against real live data.
        const bool loaded{
            _scene_runtime.load(*_game,
                                "scene.sandbox.town",
                                scene::scene_load_options_t{
                                    .apply_scene_music = false,
                                    .transition_overlay = {
                                        .style = scene::scene_transition_overlay_style_t::none
                                    }
                                })
        };
        if (!loaded)
            LOG_CORE_WARN("CarrotEditor temporary runtime harness failed to load 'scene.sandbox.town'");

        refresh_runtime_inspection();

        if (_status_summary)
        {
            _status_summary->set_text(loaded
                                          ? "Loaded temporary editor runtime scene 'scene.sandbox.town'."
                                          : "Editor runtime scene load failed; inspection panels remain idle.");
        }
    }

    void editor_app_t::on_tick(const float delta_time)
    {
        if (_game)
        {
            _game->world.update(delta_time);
            _scene_runtime.update_camera(*_game, delta_time);
            if (_scene_runtime.has_pending_scene())
                (void)_scene_runtime.update(*_game);
        }

        refresh_selected_asset_details();
        refresh_runtime_inspection();
        update_button_labels();
    }

    void editor_app_t::on_key(const events::key_event_t& e)
    {
        if (e._action != events::key_action::press && e._action != events::key_action::repeat)
            return;

        ui::ui_module_t* ui_module{ ui::ui_service_t::try_get() };
        if (!ui_module)
            return;

        switch (e._key)
        {
            case input::key_code::up:
                (void)ui_module->dispatch_navigation_action(ui::ui_navigation_action_t::move_up);
                break;
            case input::key_code::down:
                (void)ui_module->dispatch_navigation_action(ui::ui_navigation_action_t::move_down);
                break;
            case input::key_code::left:
                (void)ui_module->dispatch_navigation_action(ui::ui_navigation_action_t::move_left);
                break;
            case input::key_code::right:
                (void)ui_module->dispatch_navigation_action(ui::ui_navigation_action_t::move_right);
                break;
            case input::key_code::tab:
                if (input::has_modifier(e._mods, input::modifier::shift))
                    (void)ui_module->dispatch_navigation_action(ui::ui_navigation_action_t::focus_previous);
                else
                    (void)ui_module->dispatch_navigation_action(ui::ui_navigation_action_t::focus_next);
                break;
            case input::key_code::enter:
                (void)ui_module->dispatch_navigation_action(ui::ui_navigation_action_t::accept);
                break;
            case input::key_code::escape:
                quit_application();
                break;
            case input::key_code::f1:
                _show_debug_overlay = !_show_debug_overlay;
                if (_status_summary)
                {
                    _status_summary->set_text(_show_debug_overlay
                                                  ? "Editor debug overlay enabled."
                                                  : "Editor debug overlay hidden.");
                }
                break;
            case input::key_code::f5:
                reload_selected_asset();
                break;
            default:
                break;
        }
    }

    void editor_app_t::build_editor_ui()
    {
        ui::ui_module_t* ui_module{ ui::ui_service_t::try_get() };
        if (!ui_module)
            return;

        ui_module->set_input_ownership_mode(ui::ui_input_ownership_mode_t::ui_exclusive);

        ui::ui_root_widget_t* root{ ui_module->get_root() };
        if (!root)
            return;

        ui_module->clear_focus();
        root->remove_all_children();

        ui::ui_panel_t& background{ root->emplace_child<ui::ui_panel_t>() };
        background.set_style(background_panel_style());

        ui::ui_stack_t& screen{ root->emplace_child<ui::ui_stack_t>(ui::ui_stack_orientation_t::vertical) };
        screen.set_padding({ .left = 32.f, .top = 30.f, .right = 32.f, .bottom = 30.f });
        screen.set_spacing(18.f);
        screen.set_cross_alignment(ui::ui_stack_cross_alignment_t::stretch);

        auto& title{ screen.emplace_child<ui::ui_label_t>("CarrotEditor") };
        title.set_font_asset_id("font.engine.roboto_regular");
        title.set_font_size(27.f);
        title.set_color(color_text_primary);

        auto& subtitle{ screen.emplace_child<ui::ui_label_t>("Milestone 17 runtime iteration, scene inspection, and live world diagnostics") };
        subtitle.set_font_size(15.f);
        subtitle.set_color(color_text_secondary);

        _status_summary = &screen.emplace_child<ui::ui_label_t>("Discovering assets...");
        _status_summary->set_font_size(14.f);
        _status_summary->set_color(color_accent_glow);

        ui::ui_stack_t& body{ screen.emplace_child<ui::ui_stack_t>(ui::ui_stack_orientation_t::horizontal) };
        body.set_spacing(20.f);
        body.set_cross_alignment(ui::ui_stack_cross_alignment_t::stretch);
        body.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);

        ui::ui_panel_t& list_panel{ body.emplace_child<ui::ui_panel_t>() };
        list_panel.set_desired_size({ 410.f, 620.f });
        list_panel.set_min_size({ 360.f, 400.f });
        list_panel.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::desired);
        list_panel.set_style(chrome_panel_style(false));

        ui::ui_stack_t& list_content{ list_panel.emplace_child<ui::ui_stack_t>(ui::ui_stack_orientation_t::vertical) };
        list_content.set_spacing(14.f);
        list_content.set_cross_alignment(ui::ui_stack_cross_alignment_t::stretch);

        auto& list_title{ list_content.emplace_child<ui::ui_label_t>("Assets") };
        list_title.set_font_asset_id("font.engine.roboto_regular");
        list_title.set_font_size(19.f);
        list_title.set_color(color_text_primary);

        auto& list_hint{
            list_content.emplace_child<ui::ui_label_t>("Arrow keys navigate, Enter selects, F5 reloads, F1 toggles debug, Esc quits.")
        };
        list_hint.set_font_size(13.f);
        list_hint.set_wrap_width(332.f);
        list_hint.set_color(color_text_muted);

        _asset_list = &list_content.emplace_child<ui::ui_stack_t>(ui::ui_stack_orientation_t::vertical);
        _asset_list->set_spacing(6.f);
        _asset_list->set_cross_alignment(ui::ui_stack_cross_alignment_t::stretch);
        _asset_list->set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);

        ui::ui_panel_t& details_panel{ body.emplace_child<ui::ui_panel_t>() };
        details_panel.set_desired_size({ 470.f, 620.f });
        details_panel.set_min_size({ 420.f, 400.f });
        details_panel.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::desired);
        details_panel.set_style(chrome_panel_style(true));

        ui::ui_stack_t& details_content{ details_panel.emplace_child<ui::ui_stack_t>(ui::ui_stack_orientation_t::vertical) };
        details_content.set_spacing(12.f);
        details_content.set_cross_alignment(ui::ui_stack_cross_alignment_t::stretch);

        _details_title = &details_content.emplace_child<ui::ui_label_t>("No asset selected");
        _details_title->set_font_asset_id("font.engine.roboto_regular");
        _details_title->set_font_size(21.f);
        _details_title->set_color(color_text_primary);

        _preview_title = &details_content.emplace_child<ui::ui_label_t>("Preview unavailable");
        _preview_title->set_font_size(14.f);
        _preview_title->set_color(color_text_secondary);

        _preview_widget = &details_content.emplace_child<ui::ui_asset_preview_t>();
        _preview_widget->set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::desired);
        _preview_widget->set_style(editor_preview_style());

        _diagnostics_title = &details_content.emplace_child<ui::ui_label_t>("Diagnostics");
        _diagnostics_title->set_font_asset_id("font.engine.roboto_regular");
        _diagnostics_title->set_font_size(16.f);
        _diagnostics_title->set_color(color_accent_glow);

        _details_body = &details_content.emplace_child<ui::ui_label_t>("Choose an asset from the list to inspect runtime iteration details.");
        _details_body->set_font_size(15.f);
        _details_body->set_wrap_width(720.f);
        _details_body->set_color(color_text_secondary);
        _details_body->set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);

        _reload_button = &details_content.emplace_child<ui::ui_button_t>("Reload Selected Asset");
        _reload_button->set_style(primary_action_button_style());
        {
            ui::ui_button_text_style_t text_style;
            text_style.font_asset_id = "font.engine.roboto_regular";
            text_style.font_size = 16.f;
            text_style.horizontal_alignment = ui::ui_label_horizontal_alignment_t::center;
            _reload_button->set_text_style(text_style);
        }
        _reload_button->set_on_pressed([this]()
        {
            reload_selected_asset();
        });

        ui::ui_panel_t& scene_panel{ body.emplace_child<ui::ui_panel_t>() };
        scene_panel.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);
        scene_panel.set_style(chrome_panel_style(false));

        ui::ui_stack_t& scene_content{ scene_panel.emplace_child<ui::ui_stack_t>(ui::ui_stack_orientation_t::vertical) };
        scene_content.set_spacing(12.f);
        scene_content.set_cross_alignment(ui::ui_stack_cross_alignment_t::stretch);

        _scene_summary_title = &scene_content.emplace_child<ui::ui_label_t>("Runtime Scene");
        _scene_summary_title->set_font_asset_id("font.engine.roboto_regular");
        _scene_summary_title->set_font_size(18.f);
        _scene_summary_title->set_color(color_text_primary);

        _scene_summary_body = &scene_content.emplace_child<ui::ui_label_t>("Waiting for runtime scene summary...");
        _scene_summary_body->set_font_size(14.f);
        _scene_summary_body->set_wrap_width(560.f);
        _scene_summary_body->set_color(color_text_secondary);

        auto& systems_title{ scene_content.emplace_child<ui::ui_label_t>("Runtime Systems") };
        systems_title.set_font_asset_id("font.engine.roboto_regular");
        systems_title.set_font_size(16.f);
        systems_title.set_color(color_accent_glow);

        _systems_summary_body = &scene_content.emplace_child<ui::ui_label_t>("Waiting for runtime systems summary...");
        _systems_summary_body->set_font_size(14.f);
        _systems_summary_body->set_wrap_width(560.f);
        _systems_summary_body->set_color(color_text_secondary);

        auto& objects_title{ scene_content.emplace_child<ui::ui_label_t>("Runtime Objects") };
        objects_title.set_font_asset_id("font.engine.roboto_regular");
        objects_title.set_font_size(16.f);
        objects_title.set_color(color_accent_glow);

        _runtime_object_list = &scene_content.emplace_child<ui::ui_stack_t>(ui::ui_stack_orientation_t::vertical);
        _runtime_object_list->set_spacing(6.f);
        _runtime_object_list->set_cross_alignment(ui::ui_stack_cross_alignment_t::stretch);
        _runtime_object_list->set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);

        _object_details_title = &scene_content.emplace_child<ui::ui_label_t>("No runtime object selected");
        _object_details_title->set_font_asset_id("font.engine.roboto_regular");
        _object_details_title->set_font_size(16.f);
        _object_details_title->set_color(color_text_primary);

        _object_details_body = &scene_content.emplace_child<ui::ui_label_t>("Choose a runtime object to inspect live engine-owned state.");
        _object_details_body->set_font_size(14.f);
        _object_details_body->set_wrap_width(560.f);
        _object_details_body->set_color(color_text_secondary);

    }

    void editor_app_t::rebuild_asset_list()
    {
        if (!_game || !_asset_list)
            return;

        _asset_list->remove_all_children();
        _asset_buttons.clear();

        const std::vector<assets::asset_iteration_status_t> statuses{
            _game->assets.collect_runtime_iteration_statuses()
        };

        if (_status_summary)
        {
            _status_summary->set_text(std::format("Tracking {} runtime asset(s) across textures, sprites, and audio.", statuses.size()));
        }

        for (const assets::asset_iteration_status_t& status : statuses)
        {
            ui::ui_button_t& button{ _asset_list->emplace_child<ui::ui_button_t>(format_asset_list_label(status, false)) };
            button.set_on_pressed([this, kind = status.kind, id = status.id]()
            {
                select_asset(kind, id);
            });

            ui::ui_button_text_style_t text_style;
            text_style.font_asset_id = "font.engine.roboto_regular";
            text_style.font_size = 15.f;
            text_style.horizontal_alignment = ui::ui_label_horizontal_alignment_t::start;
            button.set_style(editor_button_style());
            button.set_text_style(text_style);
            button.set_desired_size({ 360.f, 34.f });
            button.set_min_size({ 320.f, 31.f });

            _asset_buttons.push_back({
                .kind = status.kind,
                .id = status.id,
                .button = &button
            });
        }

        if (!_asset_buttons.empty())
        {
            for (size_t i = 0; i < _asset_buttons.size(); ++i)
            {
                if (i > 0)
                    _asset_buttons[i].button->set_navigation_target(ui::ui_navigation_direction_t::up, _asset_buttons[i - 1].button);
                if (i + 1 < _asset_buttons.size())
                    _asset_buttons[i].button->set_navigation_target(ui::ui_navigation_direction_t::down, _asset_buttons[i + 1].button);
            }
        }

        if (!_has_selection && !_asset_buttons.empty())
            select_asset(_asset_buttons.front().kind, _asset_buttons.front().id);
        else
            update_button_labels();
    }

    void editor_app_t::select_asset(const assets::asset_kind_t kind, const assets::asset_id_t id)
    {
        _selected_kind = kind;
        _selected_id = id;
        _has_selection = true;

        if (ui::ui_module_t* ui_module{ ui::ui_service_t::try_get() })
        {
            if (ui::ui_button_t* button{ find_button(kind, id) })
                (void)ui_module->set_focus(button);
        }

        refresh_selected_asset_details();
        update_button_labels();
    }

    void editor_app_t::select_runtime_object(const world::world_object_id_t id)
    {
        _selected_object_id = id;
        _has_object_selection = true;

        if (ui::ui_module_t* ui_module{ ui::ui_service_t::try_get() })
        {
            if (ui::ui_button_t* button{ find_object_button(id) })
                (void)ui_module->set_focus(button);
        }
    }

    void editor_app_t::reload_selected_asset()
    {
        if (!_game || !_has_selection)
            return;

        const auto status{ _game->assets.find_runtime_iteration_status(_selected_kind, _selected_id) };
        if (!status.has_value())
            return;

        const assets::asset_runtime_refresh_action_t action{
            selected_action(*status, _scene_runtime.has_scene_loaded())
        };

        bool succeeded{ false };
        switch (action)
        {
            case assets::asset_runtime_refresh_action_t::reload_now:
            case assets::asset_runtime_refresh_action_t::reload_on_next_use:
            case assets::asset_runtime_refresh_action_t::manual_refresh:
                succeeded = _game->assets.reload_asset(_selected_kind, _selected_id);
                LOG_ASSET_INFO("Editor runtime refresh action '{}' for asset '{}' {}; reason={}",
                               assets::to_string(action),
                               status->logical_id,
                               succeeded ? "succeeded" : "failed",
                               assets::describe_runtime_refresh_action_reason(*status, _scene_runtime.has_scene_loaded()));
                break;
            case assets::asset_runtime_refresh_action_t::rebuild_current_scene:
                succeeded = _scene_runtime.request_rebuild_current_scene_for_asset(*_game, *status);
                while (succeeded && _scene_runtime.has_pending_scene())
                    succeeded = _scene_runtime.update(*_game);
                LOG_CORE_INFO("Editor runtime refresh action '{}' for asset '{}' {}; reason={}",
                              assets::to_string(action),
                              status->logical_id,
                              succeeded ? "succeeded" : "failed",
                              assets::describe_runtime_refresh_action_reason(*status, _scene_runtime.has_scene_loaded()));
                break;
            case assets::asset_runtime_refresh_action_t::restart_runtime:
            case assets::asset_runtime_refresh_action_t::none:
            default:
                LOG_CORE_WARN("Editor runtime refresh action '{}' for asset '{}' is not directly executable; reason={}",
                              assets::to_string(action),
                              status->logical_id,
                              assets::describe_runtime_refresh_action_reason(*status, _scene_runtime.has_scene_loaded()));
                break;
        }

        refresh_selected_asset_details();
        refresh_runtime_inspection();
        update_button_labels();
    }

    void editor_app_t::refresh_selected_asset_details()
    {
        if (!_details_title || !_preview_title || !_preview_widget || !_diagnostics_title || !_details_body || !_reload_button)
            return;

        if (!_game || !_has_selection)
        {
            _details_title->set_text("No asset selected");
            _details_title->set_color(0xFFFFD67Au);
            _preview_title->set_text("Preview unavailable");
            _preview_widget->clear_preview();
            _diagnostics_title->set_text("Diagnostics");
            _details_body->set_text("Choose an asset from the list to inspect runtime iteration details.");
            _reload_button->set_label("Reload Selected Asset");
            _reload_button->set_enabled(false);
            return;
        }

        const auto status{ _game->assets.find_runtime_iteration_status(_selected_kind, _selected_id) };
        if (!status)
        {
            _details_title->set_text("Selected asset no longer exists");
            _details_title->set_color(0xFFFF8C75u);
            _preview_title->set_text("Preview unavailable");
            _preview_widget->clear_preview();
            _diagnostics_title->set_text("Diagnostics");
            _details_body->set_text("The selected asset is no longer registered in the runtime asset manager.");
            _reload_button->set_label("Reload Selected Asset");
            _reload_button->set_enabled(false);
            return;
        }

        _details_title->set_text(status->logical_id);
        _details_title->set_color(status_color(*status));
        _diagnostics_title->set_text("Diagnostics");
        _details_body->set_text(format_asset_details(*status, _scene_runtime.has_scene_loaded()));
        const assets::asset_runtime_refresh_action_t action{
            selected_action(*status, _scene_runtime.has_scene_loaded())
        };
        _reload_button->set_label(std::string{ action_button_label(action) });
        _reload_button->set_enabled(can_trigger_action(action, _scene_runtime.can_request_rebuild_current_scene()));

        switch (status->kind)
        {
            case assets::asset_kind_t::texture:
                _preview_title->set_text("Texture preview");
                _preview_widget->set_texture_asset_id(status->logical_id);
                break;
            case assets::asset_kind_t::sprite:
                _preview_title->set_text("Sprite preview (first frame)");
                _preview_widget->set_sprite_asset_id(status->logical_id);
                break;
            case assets::asset_kind_t::audio:
                _preview_title->set_text("Audio preview not implemented yet");
                _preview_widget->clear_preview();
                break;
            default:
                _preview_title->set_text("Preview unavailable");
                _preview_widget->clear_preview();
                break;
        }

        if (_status_summary)
        {
            _status_summary->set_text(std::format("Selected {} asset '{}'.",
                                                  assets::to_string(status->kind),
                                                  status->logical_id));
        }
    }

    void editor_app_t::update_button_labels()
    {
        if (!_game)
            return;

        for (const asset_button_binding_t& binding : _asset_buttons)
        {
            if (!binding.button)
                continue;

            const auto status{ _game->assets.find_runtime_iteration_status(binding.kind, binding.id) };
            if (!status)
                continue;

            const bool selected{
                _has_selection &&
                binding.kind == _selected_kind &&
                binding.id == _selected_id
            };

            binding.button->set_label(format_asset_list_label(*status, selected));
        }

        const std::vector<scene::scene_runtime_object_summary_t> summaries{
            _scene_runtime.collect_runtime_object_summaries(*_game)
        };
        for (const object_button_binding_t& binding : _object_buttons)
        {
            if (!binding.button)
                continue;

            const auto it = std::ranges::find(summaries, binding.id, &scene::scene_runtime_object_summary_t::id);
            if (it == summaries.end())
                continue;

            const bool selected{ _has_object_selection && binding.id == _selected_object_id };
            binding.button->set_label(format_runtime_object_list_label(*it, selected));
        }
    }

    ui::ui_button_t* editor_app_t::find_button(const assets::asset_kind_t kind, const assets::asset_id_t id) const noexcept
    {
        for (const asset_button_binding_t& binding : _asset_buttons)
        {
            if (binding.kind == kind && binding.id == id)
                return binding.button;
        }

        return nullptr;
    }

    void editor_app_t::rebuild_runtime_object_list()
    {
        if (!_game || !_runtime_object_list)
            return;

        _runtime_object_list->remove_all_children();
        _object_buttons.clear();

        const std::vector<scene::scene_runtime_object_summary_t> summaries{
            _scene_runtime.collect_runtime_object_summaries(*_game)
        };

        for (const scene::scene_runtime_object_summary_t& summary : summaries)
        {
            ui::ui_button_t& button{ _runtime_object_list->emplace_child<ui::ui_button_t>(
                format_runtime_object_list_label(summary, _has_object_selection && summary.id == _selected_object_id)) };
            button.set_on_pressed([this, id = summary.id]()
            {
                select_runtime_object(id);
            });

            ui::ui_button_text_style_t text_style;
            text_style.font_asset_id = "font.engine.roboto_regular";
            text_style.font_size = 14.f;
            text_style.horizontal_alignment = ui::ui_label_horizontal_alignment_t::start;
            button.set_style(editor_button_style());
            button.set_text_style(text_style);
            button.set_desired_size({ 520.f, 30.f });
            button.set_min_size({ 320.f, 28.f });

            _object_buttons.push_back({
                .id = summary.id,
                .button = &button
            });
        }

        for (size_t i = 0; i < _object_buttons.size(); ++i)
        {
            if (i > 0)
                _object_buttons[i].button->set_navigation_target(ui::ui_navigation_direction_t::up, _object_buttons[i - 1].button);
            if (i + 1 < _object_buttons.size())
                _object_buttons[i].button->set_navigation_target(ui::ui_navigation_direction_t::down, _object_buttons[i + 1].button);
        }

        if (!_has_object_selection && !_object_buttons.empty())
        {
            _selected_object_id = _object_buttons.front().id;
            _has_object_selection = true;
        }
    }

    void editor_app_t::refresh_runtime_inspection()
    {
        if (!_game || !_scene_summary_body || !_systems_summary_body || !_object_details_title || !_object_details_body)
            return;

        const scene::scene_runtime_summary_t scene_summary{ _scene_runtime.summarize(*_game) };
        const scene::scene_runtime_systems_summary_t systems_summary{ _scene_runtime.summarize_runtime_systems(*_game) };
        _scene_summary_body->set_text(format_scene_summary(scene_summary));
        _systems_summary_body->set_text(format_systems_summary(systems_summary));

        rebuild_runtime_object_list();

        if (!_has_object_selection)
        {
            _object_details_title->set_text("No runtime object selected");
            _object_details_body->set_text("Choose a runtime object to inspect live engine-owned state.");
            return;
        }

        const auto selected{ _scene_runtime.find_runtime_object_summary(*_game, _selected_object_id) };
        if (!selected)
        {
            _object_details_title->set_text("Runtime object no longer exists");
            _object_details_body->set_text("The selected runtime object is no longer present in the live world.");
            _has_object_selection = false;
            _selected_object_id = 0;
            return;
        }

        _object_details_title->set_text(selected->name.empty() ? "Selected Runtime Object" : selected->name);
        _object_details_body->set_text(format_runtime_object_details(*selected));
    }

    ui::ui_button_t* editor_app_t::find_object_button(const world::world_object_id_t id) const noexcept
    {
        for (const object_button_binding_t& binding : _object_buttons)
        {
            if (binding.id == id)
                return binding.button;
        }

        return nullptr;
    }
} // namespace carrot::editor
