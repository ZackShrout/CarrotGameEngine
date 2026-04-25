//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "GameContext.h"
#include "CoreDefines.h"
#include "Events/Events.h"
#include "Save/SaveService.h"
#include "Window/Window.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace carrot::core {
    class igame_state_t;

    class game_runtime_t
    {
    public:
        DISABLE_COPY_AND_MOVE(game_runtime_t)

        explicit game_runtime_t(game_context_t& game) noexcept;
        virtual ~game_runtime_t();

        virtual void start() {}
        virtual void tick(float delta_time);
        virtual void render_overlay();
        virtual void on_window_focus_changed(const events::window_focused_t& e);
        virtual void on_key(const events::key_event_t& e);
        virtual void on_mouse_moved(const events::mouse_moved_event_t& e);
        virtual void on_mouse_button(const events::mouse_button_event_t& e);
        virtual void on_mouse_scrolled(const events::mouse_scrolled_event_t& e);

        void set_active_state(std::unique_ptr<igame_state_t> state);
        void clear_active_state();

        [[nodiscard]] igame_state_t* active_state() noexcept { return _active_state.get(); }
        [[nodiscard]] const igame_state_t* active_state() const noexcept { return _active_state.get(); }
        [[nodiscard]] game_context_t& game() noexcept { return _game; }
        [[nodiscard]] const game_context_t& game() const noexcept { return _game; }
        [[nodiscard]] bool has_save_service() const noexcept { return _game.save_service != nullptr; }
        [[nodiscard]] std::vector<save::save_slot_summary_t> list_save_slots() const;
        [[nodiscard]] const save::save_operation_status_t& save_operation_status() const noexcept
        {
            return _save_status;
        }
        void clear_save_operation_status() noexcept;
        [[nodiscard]] bool request_save(std::string_view slot_id,
                                        save::save_slot_kind_t slot_kind = save::save_slot_kind_t::manual);
        [[nodiscard]] bool request_manual_save(std::string_view slot_id);
        [[nodiscard]] bool request_autosave(std::string_view slot_id = {});
        [[nodiscard]] bool request_temp_save(std::string_view slot_id = {});
        [[nodiscard]] bool request_load(std::string_view slot_id);
        [[nodiscard]] bool request_load_most_recent(save::save_slot_kind_t slot_kind);
        [[nodiscard]] std::vector<save::save_slot_summary_t> list_save_slots(save::save_slot_kind_t slot_kind) const;
        [[nodiscard]] std::optional<save::save_slot_summary_t> most_recent_save_slot(save::save_slot_kind_t slot_kind) const;
        [[nodiscard]] bool map_collision_debug_visible() const noexcept;
        [[nodiscard]] bool object_collider_debug_visible() const noexcept;
        [[nodiscard]] bool trigger_volume_debug_visible() const noexcept;
        void set_map_collision_debug_visible(bool visible) noexcept;
        void set_object_collider_debug_visible(bool visible) noexcept;
        void set_trigger_volume_debug_visible(bool visible) noexcept;
        [[nodiscard]] bool toggle_map_collision_debug() noexcept;
        [[nodiscard]] bool toggle_object_collider_debug() noexcept;
        [[nodiscard]] bool toggle_trigger_volume_debug() noexcept;

        [[nodiscard]] static bool is_fullscreen() { return window::is_fullscreen(); }
        static void set_fullscreen(bool fullscreen) noexcept { window::set_fullscreen(fullscreen); }
        static void quit_application() { window::set_should_close(true); }

    protected:
        game_runtime_t() = delete;

    private:
        [[nodiscard]] static std::string_view default_slot_id_for(save::save_slot_kind_t slot_kind) noexcept;
        class runtime_engine_save_participant_t final : public save::isave_participant_t
        {
        public:
            explicit runtime_engine_save_participant_t(game_runtime_t& runtime) noexcept
                : _runtime(runtime) {}

            [[nodiscard]] std::string_view participant_name() const noexcept override
            {
                return "engine.runtime";
            }

            [[nodiscard]] save::save_section_owner_t owner() const noexcept override
            {
                return save::save_section_owner_t::engine;
            }

            bool capture_save_sections(const save::save_request_t& request,
                                       save::save_section_collector_t& collector,
                                       save::save_operation_status_t& status) override;
            bool apply_loaded_sections(const save::loaded_save_slot_t& slot,
                                       save::save_operation_status_t& status) override;

        private:
            game_runtime_t& _runtime;
        };

        [[nodiscard]] bool queue_save_request(save::save_request_kind_t kind,
                                             std::string_view slot_id,
                                             save::save_slot_kind_t slot_kind);
        void process_pending_save_request() noexcept;
        void build_save_participant_registry(save::save_participant_registry_t& registry);

        game_context_t& _game;
        std::unique_ptr<igame_state_t> _active_state;
        std::optional<save::save_request_t> _pending_save_request;
        save::save_operation_status_t _save_status;
        runtime_engine_save_participant_t _runtime_save_participant{ *this };
    };
} // namespace carrot::core
