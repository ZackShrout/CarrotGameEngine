//
// Created by Zack Shrout on 4/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

#include <memory>
#include <vector>

namespace carrot::editor {
    class editor_app_t final : public core::ce_application_t
    {
    public:
        editor_app_t() = default;
        ~editor_app_t() override = default;

        void describe_boot_prewarm(core::boot_prewarm_plan_t& plan) const override;
        void start(core::game_context_t& game) override;
        void on_tick(float delta_time) override;
        void on_key(const events::key_event_t& e) override;
        [[nodiscard]] bool show_debug_overlay() const noexcept override { return _show_debug_overlay; }

    private:
        struct asset_button_binding_t
        {
            assets::asset_kind_t kind{ assets::asset_kind_t::texture };
            assets::asset_id_t id{ 0 };
            ui::ui_button_t* button{ nullptr };
        };

        void build_editor_ui();
        void rebuild_asset_list();
        void select_asset(assets::asset_kind_t kind, assets::asset_id_t id);
        void reload_selected_asset();
        void refresh_selected_asset_details();
        void update_button_labels();
        [[nodiscard]] ui::ui_button_t* find_button(assets::asset_kind_t kind, assets::asset_id_t id) const noexcept;

        core::game_context_t* _game{ nullptr };
        ui::ui_stack_t* _asset_list{ nullptr };
        ui::ui_label_t* _status_summary{ nullptr };
        ui::ui_label_t* _details_title{ nullptr };
        ui::ui_label_t* _preview_title{ nullptr };
        ui::ui_asset_preview_t* _preview_widget{ nullptr };
        ui::ui_label_t* _diagnostics_title{ nullptr };
        ui::ui_label_t* _details_body{ nullptr };
        ui::ui_button_t* _reload_button{ nullptr };
        assets::asset_kind_t _selected_kind{ assets::asset_kind_t::texture };
        assets::asset_id_t _selected_id{ 0 };
        bool _has_selection{ false };
        bool _show_debug_overlay{ false };
        std::vector<asset_button_binding_t> _asset_buttons;
    };
} // namespace carrot::editor
