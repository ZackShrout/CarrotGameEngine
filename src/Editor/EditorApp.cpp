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
                case assets::asset_kind_t::texture: return "T";
                case assets::asset_kind_t::sprite: return "S";
                case assets::asset_kind_t::audio: return "A";
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

        [[nodiscard]] std::string format_asset_details(const assets::asset_iteration_status_t& status)
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

            append_line("Kind", assets::to_string(status.kind));
            append_line("Reload Policy", assets::to_string(status.reload_policy));
            append_line("Source", status.source_uri);
            append_line("Manifest", status.manifest_uri.empty() ? std::string_view{ "<none>" } : std::string_view{ status.manifest_uri });
            append_line("Cached In Runtime", status.loaded_in_runtime_cache ? std::string_view{ "yes" } : std::string_view{ "no" });

            if (!status.has_last_attempt)
            {
                append_line("Last Attempt", "never");
                return text;
            }

            append_line("Last Attempt", assets::to_string(status.last_result));
            append_line("Load Origin", assets::to_string(status.last_load_origin));
            append_line("Cooked Artifact", assets::to_string(status.last_cooked_artifact_state));
            append_line("Invalidation Reason", assets::to_string(status.last_invalidation_reason));
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

        [[nodiscard]] std::string_view reload_button_label(const assets::asset_reload_policy_t policy) noexcept
        {
            switch (policy)
            {
                case assets::asset_reload_policy_t::manual_refresh_only: return "Refresh Selected Asset";
                case assets::asset_reload_policy_t::restart_or_scene_rebuild_required: return "Reload Requires Restart";
                case assets::asset_reload_policy_t::reloadable_live:
                case assets::asset_reload_policy_t::reloadable_on_next_use:
                default:
                    return "Reload Selected Asset";
            }
        }

        [[nodiscard]] bool can_trigger_reload(const assets::asset_reload_policy_t policy) noexcept
        {
            return policy != assets::asset_reload_policy_t::restart_or_scene_rebuild_required;
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
    }

    void editor_app_t::on_tick([[maybe_unused]] const float delta_time)
    {
        refresh_selected_asset_details();
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

        auto& subtitle{ screen.emplace_child<ui::ui_label_t>("Milestone 13 asset browser and runtime diagnostics") };
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
        details_panel.set_main_axis_size_rule(ui::ui_main_axis_size_rule_t::flex);
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

        (void)ui_module->focus_first();
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

    void editor_app_t::reload_selected_asset()
    {
        if (!_game || !_has_selection)
            return;

        const bool reloaded{ _game->assets.reload_asset(_selected_kind, _selected_id) };
        LOG_ASSET_INFO("Editor manual reload for '{}' {}",
                       _has_selection ? "selected asset" : "asset",
                       reloaded ? "succeeded" : "failed");
        refresh_selected_asset_details();
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
        _details_body->set_text(format_asset_details(*status));
        _reload_button->set_label(std::string{ reload_button_label(status->reload_policy) });
        _reload_button->set_enabled(can_trigger_reload(status->reload_policy));

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
} // namespace carrot::editor
