//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "GameRuntime.h"

#include "GameContext.h"
#include "GameState.h"
#include "Scene/Scene.h"
#include "Utils/File/FileUtils.h"

namespace carrot::core {
    namespace {
        constexpr std::string_view k_runtime_engine_section_id{ "engine_runtime" };
        constexpr std::string_view k_default_autosave_slot_id{ "autosave" };
        constexpr std::string_view k_default_temp_slot_id{ "continue" };

        [[nodiscard]] std::span<const std::uint8_t> as_bytes(const auto& value) noexcept
        {
            return {
                reinterpret_cast<const std::uint8_t*>(&value),
                sizeof(value)
            };
        }
    } // namespace

    game_runtime_t::game_runtime_t(game_context_t& game) noexcept
        : _game(game)
    {
    }

    game_runtime_t::~game_runtime_t()
    {
        clear_active_state();
    }

    void game_runtime_t::tick(const float delta_time)
    {
        process_pending_save_request();

        if (_active_state)
        {
            _active_state->tick(delta_time);
            if (scene::scene_runtime_t* runtime{ _active_state->scene_runtime() })
                runtime->advance_transition_overlay(delta_time);
        }
    }

    void game_runtime_t::render_overlay()
    {
        if (_active_state)
        {
            if (scene::scene_runtime_t* runtime{ _active_state->scene_runtime() })
                runtime->render_transition_overlay(_game);
            _active_state->render_overlay();
        }
    }

    void game_runtime_t::on_window_focus_changed(const events::window_focused_t& e)
    {
        if (_active_state)
            _active_state->on_window_focus_changed(e);
    }

    void game_runtime_t::on_key(const events::key_event_t& e)
    {
        if (_active_state)
            _active_state->on_key(e);
    }

    void game_runtime_t::on_mouse_moved(const events::mouse_moved_event_t& e)
    {
        if (_active_state)
            _active_state->on_mouse_moved(e);
    }

    void game_runtime_t::on_mouse_button(const events::mouse_button_event_t& e)
    {
        if (_active_state)
            _active_state->on_mouse_button(e);
    }

    void game_runtime_t::on_mouse_scrolled(const events::mouse_scrolled_event_t& e)
    {
        if (_active_state)
            _active_state->on_mouse_scrolled(e);
    }

    std::vector<save::save_slot_summary_t> game_runtime_t::list_save_slots() const
    {
        if (!_game.save_service)
            return {};

        return _game.save_service->list_slots();
    }

    void game_runtime_t::clear_save_operation_status() noexcept
    {
        _save_status = save::save_operation_status_t{};
    }

    bool game_runtime_t::request_save(const std::string_view slot_id, const save::save_slot_kind_t slot_kind)
    {
        return queue_save_request(save::save_request_kind_t::save_slot, slot_id, slot_kind);
    }

    bool game_runtime_t::request_manual_save(const std::string_view slot_id)
    {
        return request_save(slot_id, save::save_slot_kind_t::manual);
    }

    bool game_runtime_t::request_autosave(const std::string_view slot_id)
    {
        return request_save(slot_id.empty() ? default_slot_id_for(save::save_slot_kind_t::autosave) : slot_id,
                            save::save_slot_kind_t::autosave);
    }

    bool game_runtime_t::request_temp_save(const std::string_view slot_id)
    {
        return request_save(slot_id.empty() ? default_slot_id_for(save::save_slot_kind_t::temp) : slot_id,
                            save::save_slot_kind_t::temp);
    }

    bool game_runtime_t::request_load(const std::string_view slot_id)
    {
        return queue_save_request(save::save_request_kind_t::load_slot, slot_id, save::save_slot_kind_t::manual);
    }

    bool game_runtime_t::request_load_most_recent(const save::save_slot_kind_t slot_kind)
    {
        const std::optional<save::save_slot_summary_t> slot{ most_recent_save_slot(slot_kind) };
        if (!slot.has_value())
        {
            _save_status = save::save_operation_status_t{
                .request_kind = save::save_request_kind_t::load_slot,
                .outcome = save::save_request_outcome_t::failed,
                .slot_id = std::string{ default_slot_id_for(slot_kind) },
                .detail = std::format("No {} save slot is available to load.", save::to_string(slot_kind))
            };
            return false;
        }

        return request_load(slot->slot_id);
    }

    std::vector<save::save_slot_summary_t> game_runtime_t::list_save_slots(const save::save_slot_kind_t slot_kind) const
    {
        std::vector<save::save_slot_summary_t> slots{ list_save_slots() };
        std::erase_if(slots, [slot_kind](const save::save_slot_summary_t& slot)
        {
            return slot.metadata.slot_kind != slot_kind;
        });
        return slots;
    }

    std::optional<save::save_slot_summary_t> game_runtime_t::most_recent_save_slot(
        const save::save_slot_kind_t slot_kind) const
    {
        std::vector<save::save_slot_summary_t> slots{ list_save_slots(slot_kind) };
        if (slots.empty())
            return std::nullopt;

        std::ranges::sort(slots, [](const save::save_slot_summary_t& lhs, const save::save_slot_summary_t& rhs)
        {
            if (lhs.metadata.timestamp_utc_unix_seconds != rhs.metadata.timestamp_utc_unix_seconds)
                return lhs.metadata.timestamp_utc_unix_seconds > rhs.metadata.timestamp_utc_unix_seconds;

            return lhs.slot_id < rhs.slot_id;
        });
        return slots.front();
    }

    bool game_runtime_t::map_collision_debug_visible() const noexcept
    {
        return _game.world.collision_debug_view().show_map_collision;
    }

    bool game_runtime_t::object_collider_debug_visible() const noexcept
    {
        return _game.world.collision_debug_view().show_object_colliders;
    }

    bool game_runtime_t::trigger_volume_debug_visible() const noexcept
    {
        return _game.world.collision_debug_view().show_trigger_volumes;
    }

    void game_runtime_t::set_map_collision_debug_visible(const bool visible) noexcept
    {
        _game.world.collision_debug_view().show_map_collision = visible;
    }

    void game_runtime_t::set_object_collider_debug_visible(const bool visible) noexcept
    {
        _game.world.collision_debug_view().show_object_colliders = visible;
    }

    void game_runtime_t::set_trigger_volume_debug_visible(const bool visible) noexcept
    {
        _game.world.collision_debug_view().show_trigger_volumes = visible;
    }

    bool game_runtime_t::toggle_map_collision_debug() noexcept
    {
        const bool next_visible{ !map_collision_debug_visible() };
        set_map_collision_debug_visible(next_visible);
        return next_visible;
    }

    bool game_runtime_t::toggle_object_collider_debug() noexcept
    {
        const bool next_visible{ !object_collider_debug_visible() };
        set_object_collider_debug_visible(next_visible);
        return next_visible;
    }

    bool game_runtime_t::toggle_trigger_volume_debug() noexcept
    {
        const bool next_visible{ !trigger_volume_debug_visible() };
        set_trigger_volume_debug_visible(next_visible);
        return next_visible;
    }

    void game_runtime_t::set_active_state(std::unique_ptr<igame_state_t> state)
    {
        if (_active_state)
            _active_state->exit();

        _active_state = std::move(state);

        if (_active_state)
            _active_state->enter();
    }

    void game_runtime_t::clear_active_state()
    {
        if (_active_state)
            _active_state->exit();

        _active_state.reset();
    }

    std::string_view game_runtime_t::default_slot_id_for(const save::save_slot_kind_t slot_kind) noexcept
    {
        switch (slot_kind)
        {
            case save::save_slot_kind_t::manual: return {};
            case save::save_slot_kind_t::autosave: return k_default_autosave_slot_id;
            case save::save_slot_kind_t::temp: return k_default_temp_slot_id;
        }

        return {};
    }

    bool game_runtime_t::queue_save_request(const save::save_request_kind_t kind,
                                            const std::string_view slot_id,
                                            const save::save_slot_kind_t slot_kind)
    {
        if (!_game.save_service)
        {
            _save_status = save::save_operation_status_t{
                .request_kind = kind,
                .outcome = save::save_request_outcome_t::failed,
                .slot_id = std::string{ slot_id },
                .detail = "Save service is not configured for this runtime."
            };
            return false;
        }

        if (_pending_save_request.has_value())
        {
            _save_status = save::save_operation_status_t{
                .request_kind = kind,
                .outcome = save::save_request_outcome_t::failed,
                .slot_id = std::string{ slot_id },
                .detail = "A save/load request is already pending."
            };
            return false;
        }

        save::save_request_t request{
            .kind = kind,
            .slot_id = std::string{ slot_id },
            .slot_kind = slot_kind
        };
        if (kind == save::save_request_kind_t::save_slot && _active_state)
        {
            if (scene::scene_runtime_t* runtime{ _active_state->scene_runtime() };
                runtime != nullptr && runtime->has_scene_loaded())
            {
                request.scene_id = std::string{ runtime->current_scene_id() };
                request.scene_label = request.scene_id;
                request.spawn_marker = std::string{ runtime->current_spawn_marker() };
            }
        }

        _pending_save_request = std::move(request);
        _save_status = save::save_operation_status_t{
            .request_kind = kind,
            .outcome = save::save_request_outcome_t::pending,
            .slot_id = std::string{ slot_id },
            .detail = "Queued for runtime processing."
        };
        return true;
    }

    void game_runtime_t::process_pending_save_request() noexcept
    {
        if (!_pending_save_request.has_value())
            return;

        if (!_game.save_service)
        {
            _save_status = save::save_operation_status_t{
                .request_kind = _pending_save_request->kind,
                .outcome = save::save_request_outcome_t::failed,
                .slot_id = _pending_save_request->slot_id,
                .detail = "Save service is not configured for this runtime."
            };
            _pending_save_request.reset();
            return;
        }

        save::save_participant_registry_t registry;
        build_save_participant_registry(registry);

        if (_pending_save_request->kind == save::save_request_kind_t::save_slot)
        {
            save::save_section_collector_t collector;
            for (auto& participant_ref : registry.participants())
            {
                save::isave_participant_t& participant{ participant_ref.get() };
                save::save_operation_status_t participant_status{
                    .request_kind = _pending_save_request->kind,
                    .outcome = save::save_request_outcome_t::pending,
                    .slot_id = _pending_save_request->slot_id
                };
                if (!participant.capture_save_sections(*_pending_save_request, collector, participant_status))
                {
                    participant_status.outcome = save::save_request_outcome_t::failed;
                    if (participant_status.detail.empty())
                    {
                        participant_status.detail = std::format("Save participant '{}' failed to capture state.",
                                                                participant.participant_name());
                    }
                    _save_status = std::move(participant_status);
                    _pending_save_request.reset();
                    return;
                }
            }

            _save_status = _game.save_service->write_slot(*_pending_save_request, collector.sections());
        }
        else
        {
            save::save_operation_status_t load_status;
            const std::optional<save::loaded_save_slot_t> loaded_slot{
                _game.save_service->load_slot(_pending_save_request->slot_id, load_status)
            };
            _save_status = std::move(load_status);
            if (_save_status.outcome == save::save_request_outcome_t::succeeded && loaded_slot.has_value())
            {
                for (auto& participant_ref : registry.participants())
                {
                    save::isave_participant_t& participant{ participant_ref.get() };
                    save::save_operation_status_t participant_status{
                        .request_kind = _pending_save_request->kind,
                        .outcome = save::save_request_outcome_t::pending,
                        .slot_id = _pending_save_request->slot_id
                    };

                    if (!participant.apply_loaded_sections(*loaded_slot, participant_status))
                    {
                        participant_status.outcome = save::save_request_outcome_t::failed;
                        if (participant_status.detail.empty())
                        {
                            participant_status.detail = std::format("Save participant '{}' failed to apply state.",
                                                                    participant.participant_name());
                        }
                        _save_status = std::move(participant_status);
                        _pending_save_request.reset();
                        return;
                    }
                }
            }
        }

        _pending_save_request.reset();
    }

    void game_runtime_t::build_save_participant_registry(save::save_participant_registry_t& registry)
    {
        registry.add(_runtime_save_participant);
        if (_active_state)
            _active_state->register_save_participants(registry);
    }

    bool game_runtime_t::runtime_engine_save_participant_t::capture_save_sections(
        const save::save_request_t& request,
        save::save_section_collector_t& collector,
        save::save_operation_status_t& status)
    {
        (void)request;

        struct engine_runtime_save_data_t
        {
            std::uint8_t map_collision_debug_visible{ 0u };
            std::uint8_t object_collider_debug_visible{ 0u };
            std::uint8_t trigger_volume_debug_visible{ 0u };
            std::uint8_t reserved0{ 0u };
        };

        const engine_runtime_save_data_t data{
            .map_collision_debug_visible = static_cast<std::uint8_t>(_runtime.map_collision_debug_visible()),
            .object_collider_debug_visible = static_cast<std::uint8_t>(_runtime.object_collider_debug_visible()),
            .trigger_volume_debug_visible = static_cast<std::uint8_t>(_runtime.trigger_volume_debug_visible())
        };

        if (!collector.add_section(k_runtime_engine_section_id, owner(), as_bytes(data), status.detail))
            return false;

        status.detail = "Captured engine runtime save sections.";
        return true;
    }

    bool game_runtime_t::runtime_engine_save_participant_t::apply_loaded_sections(
        const save::loaded_save_slot_t& slot,
        save::save_operation_status_t& status)
    {
        struct engine_runtime_save_data_t
        {
            std::uint8_t map_collision_debug_visible{ 0u };
            std::uint8_t object_collider_debug_visible{ 0u };
            std::uint8_t trigger_volume_debug_visible{ 0u };
            std::uint8_t reserved0{ 0u };
        };

        const save::save_payload_section_t* section{ slot.find_section(k_runtime_engine_section_id) };
        if (!section)
            return true;

        if (section->owner != owner())
        {
            status.detail = "Engine runtime save section owner mismatch.";
            return false;
        }

        if (section->bytes.size() != sizeof(engine_runtime_save_data_t))
        {
            status.detail = "Engine runtime save section payload size is invalid.";
            return false;
        }

        engine_runtime_save_data_t data{};
        std::memcpy(&data, section->bytes.data(), sizeof(data));
        _runtime.set_map_collision_debug_visible(data.map_collision_debug_visible != 0u);
        _runtime.set_object_collider_debug_visible(data.object_collider_debug_visible != 0u);
        _runtime.set_trigger_volume_debug_visible(data.trigger_volume_debug_visible != 0u);
        status.detail = "Applied engine runtime save sections.";
        return true;
    }
} // namespace carrot::core
