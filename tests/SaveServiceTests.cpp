//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Assets/AssetManager.h"
#include "Core/GameContext.h"
#include "Core/GameRuntime.h"
#include "Core/GameState.h"
#include "Core/GameView.h"
#include "EngineConfig.h"
#include "IO/VirtualFileSystem.h"
#include "Renderer/Renderer.h"
#include "RHI/RHI.h"
#include "Save/SaveService.h"
#include "Utils/File/FileUtils.h"
#include "Window/Window.h"
#include "World/World.h"

#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <span>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        class recording_save_state_t final : public core::igame_state_t
        {
        public:
            explicit recording_save_state_t(core::game_runtime_t& runtime) noexcept
                : igame_state_t(runtime)
                , participant{ *this } {}

            void tick(float) override {}

            void register_save_participants(save::save_participant_registry_t& registry) override
            {
                registry.add(participant);
            }

            class gameplay_participant_t final : public save::isave_participant_t
            {
            public:
                explicit gameplay_participant_t(recording_save_state_t& owner) noexcept
                    : _owner(owner) {}

                [[nodiscard]] std::string_view participant_name() const noexcept override
                {
                    return "gameplay.recording";
                }

                [[nodiscard]] save::save_section_owner_t owner() const noexcept override
                {
                    return save::save_section_owner_t::gameplay;
                }

                bool capture_save_sections(const save::save_request_t& request,
                                           save::save_section_collector_t& collector,
                                           save::save_operation_status_t& status) override
                {
                    ++_owner.capture_call_count;
                    _owner.last_captured_slot_id = request.slot_id;
                    if (!_owner.capture_should_succeed)
                    {
                        status.detail = "capture failed on purpose";
                        return false;
                    }

                    const std::span<const std::uint8_t> payload{
                        reinterpret_cast<const std::uint8_t*>(_owner.captured_payload.data()),
                        _owner.captured_payload.size()
                    };
                    if (!collector.add_section("gameplay_state", owner(), payload, status.detail))
                        return false;

                    status.detail = "gameplay participant captured";
                    return true;
                }

                bool apply_loaded_sections(const save::loaded_save_slot_t& slot,
                                           save::save_operation_status_t& status) override
                {
                    ++_owner.apply_call_count;
                    _owner.last_applied_slot_id = slot.metadata.slot_id;
                    if (!_owner.apply_should_succeed)
                    {
                        status.detail = "apply failed on purpose";
                        return false;
                    }

                    const save::save_payload_section_t* section{ slot.find_section("gameplay_state") };
                    if (!section)
                    {
                        status.detail = "missing gameplay_state section";
                        return false;
                    }

                    if (section->owner != owner())
                    {
                        status.detail = "gameplay_state section owner mismatch";
                        return false;
                    }

                    _owner.applied_payload.assign(reinterpret_cast<const char*>(section->bytes.data()),
                                                  section->bytes.size());
                    status.detail = "gameplay participant applied";
                    return true;
                }

            private:
                recording_save_state_t& _owner;
            };

            bool capture_should_succeed{ true };
            bool apply_should_succeed{ true };
            size_t capture_call_count{ 0u };
            size_t apply_call_count{ 0u };
            std::string last_captured_slot_id;
            std::string last_applied_slot_id;
            std::string captured_payload{ "inventory=3;flags=opened_chest" };
            std::string applied_payload;
            gameplay_participant_t participant;
        };

        [[nodiscard]] std::filesystem::path temp_root(const std::string_view name)
        {
            return std::filesystem::temp_directory_path() / "carrot_save_service_tests" / std::string{ name };
        }

        void reset_temp_root(const std::string_view name)
        {
            std::error_code ec;
            std::filesystem::remove_all(temp_root(name), ec);
            std::filesystem::create_directories(temp_root(name) / "save", ec);
        }

        class runtime_test_context_t
        {
        public:
            explicit runtime_test_context_t(const std::string_view root_name)
                : renderer{ vfs, graphics_config, window::invalid_window_id }
                , assets{ vfs, *renderer.get_rhi() }
                , view{ renderer }
                , save_service{ vfs }
                , game{
                    .world = world,
                    .assets = assets,
                    .view = view,
                    .controllers = controllers,
                    .save_service = &save_service
                }
                , runtime{ game }
                , _root_name{ root_name }
            {
                reset_temp_root(_root_name);
                vfs.mount("save", temp_root(_root_name) / "save", false);
            }

            ~runtime_test_context_t()
            {
                std::error_code ec;
                std::filesystem::remove_all(temp_root(_root_name), ec);
            }

            io::virtual_file_system_t vfs;
            const engine_graphics_config_t graphics_config{
                .api = rhi::graphics_api::null_backend,
                .enable_debug_layers = false
            };
            renderer::renderer_t renderer;
            assets::asset_manager_t assets;
            world::world_t world;
            core::game_view_t view;
            input::controller_manager_t controllers;
            save::save_service_t save_service;
            core::game_context_t game;
            core::game_runtime_t runtime;

        private:
            std::string _root_name;
        };

        void test_save_service_lists_slots_after_save_request()
        {
            constexpr std::string_view k_root_name{ "direct_slot_listing" };
            reset_temp_root(k_root_name);

            io::virtual_file_system_t vfs;
            vfs.mount("save", temp_root(k_root_name) / "save", false);
            save::save_service_t service{ vfs };

            CARROT_TEST_REQUIRE(service.configured());
            CARROT_TEST_REQUIRE(service.list_slots().empty());

            const save::save_operation_status_t save_result{
                service.write_slot(save::save_request_t{
                    .kind = save::save_request_kind_t::save_slot,
                    .slot_id = "manual_001",
                    .slot_kind = save::save_slot_kind_t::manual,
                    .scene_id = "scene.sandbox.town",
                    .scene_label = "scene.sandbox.town",
                    .spawn_marker = "PlayerSpawn",
                    .playtime_seconds = 12.5
                }, std::span<const save::save_payload_section_t>{})
            };
            CARROT_TEST_REQUIRE(save_result.outcome == save::save_request_outcome_t::succeeded);

            const std::vector<save::save_slot_summary_t> slots{ service.list_slots() };
            CARROT_TEST_REQUIRE(slots.size() == 1u);
            CARROT_TEST_REQUIRE(slots.front().slot_id == "manual_001");
            CARROT_TEST_REQUIRE(std::filesystem::exists(slots.front().native_path / "metadata.json"));
            CARROT_TEST_REQUIRE(std::filesystem::exists(slots.front().native_path / "payload.bin"));
            CARROT_TEST_REQUIRE(slots.front().metadata.slot_kind == save::save_slot_kind_t::manual);
            CARROT_TEST_REQUIRE(slots.front().metadata.scene_id == "scene.sandbox.town");
            CARROT_TEST_REQUIRE(slots.front().metadata.spawn_marker == "PlayerSpawn");
            CARROT_TEST_REQUIRE(slots.front().metadata.playtime_seconds == 12.5);
            CARROT_TEST_REQUIRE(slots.front().metadata.payload_file == "payload.bin");
            CARROT_TEST_REQUIRE(slots.front().metadata.payload_size_bytes > 0u);

            const std::optional<save::save_slot_metadata_t> metadata{ service.read_slot_metadata("manual_001") };
            CARROT_TEST_REQUIRE(metadata.has_value());
            CARROT_TEST_REQUIRE(metadata->slot_id == "manual_001");
            CARROT_TEST_REQUIRE(metadata->payload_format_version == 1u);

            save::save_operation_status_t load_result;
            const std::optional<save::loaded_save_slot_t> loaded_slot{ service.load_slot("manual_001", load_result) };
            CARROT_TEST_REQUIRE(load_result.outcome == save::save_request_outcome_t::succeeded);
            CARROT_TEST_REQUIRE(loaded_slot.has_value());
            CARROT_TEST_REQUIRE(loaded_slot->sections.empty());

            const save::save_operation_status_t missing_result{
                service.execute(save::save_request_t{
                    .kind = save::save_request_kind_t::load_slot,
                    .slot_id = "missing_slot"
                })
            };
            CARROT_TEST_REQUIRE(missing_result.outcome == save::save_request_outcome_t::failed);

            std::error_code ec;
            std::filesystem::remove_all(temp_root(k_root_name), ec);
        }

        void test_game_runtime_processes_queued_save_requests()
        {
            runtime_test_context_t context{ "runtime_save_requests" };
            auto state{ std::make_unique<recording_save_state_t>(context.runtime) };
            recording_save_state_t* state_ptr{ state.get() };
            context.runtime.set_active_state(std::move(state));

            CARROT_TEST_REQUIRE(context.runtime.request_save("autosave", save::save_slot_kind_t::autosave));
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::pending);

            context.runtime.tick(0.f);

            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::succeeded);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().slot_id == "autosave");
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().detail == "Prepared save slot 'autosave' with metadata and payload container.");
            CARROT_TEST_REQUIRE(state_ptr->capture_call_count == 1u);
            CARROT_TEST_REQUIRE(state_ptr->last_captured_slot_id == "autosave");

            const std::vector<save::save_slot_summary_t> slots{ context.runtime.list_save_slots() };
            CARROT_TEST_REQUIRE(slots.size() == 1u);
            CARROT_TEST_REQUIRE(slots.front().slot_id == "autosave");
            CARROT_TEST_REQUIRE(slots.front().metadata.slot_kind == save::save_slot_kind_t::autosave);
            save::save_operation_status_t direct_load_status;
            const std::optional<save::loaded_save_slot_t> direct_loaded_slot{
                context.save_service.load_slot("autosave", direct_load_status)
            };
            CARROT_TEST_REQUIRE(direct_load_status.outcome == save::save_request_outcome_t::succeeded);
            CARROT_TEST_REQUIRE(direct_loaded_slot.has_value());
            const save::save_payload_section_t* gameplay_section{ direct_loaded_slot->find_section("gameplay_state") };
            CARROT_TEST_REQUIRE(gameplay_section != nullptr);
            CARROT_TEST_REQUIRE(gameplay_section->owner == save::save_section_owner_t::gameplay);
            const std::string gameplay_payload{
                reinterpret_cast<const char*>(gameplay_section->bytes.data()),
                gameplay_section->bytes.size()
            };
            CARROT_TEST_REQUIRE(gameplay_payload == state_ptr->captured_payload);
            const save::save_payload_section_t* engine_section{ direct_loaded_slot->find_section("engine_runtime") };
            CARROT_TEST_REQUIRE(engine_section != nullptr);
            CARROT_TEST_REQUIRE(engine_section->owner == save::save_section_owner_t::engine);

            CARROT_TEST_REQUIRE(context.runtime.request_load("autosave"));
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::pending);

            context.runtime.tick(0.f);

            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::succeeded);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().detail == "Resolved save slot 'autosave' for load.");
            CARROT_TEST_REQUIRE(state_ptr->apply_call_count == 1u);
            CARROT_TEST_REQUIRE(state_ptr->last_applied_slot_id == "autosave");
            CARROT_TEST_REQUIRE(state_ptr->applied_payload == state_ptr->captured_payload);

            CARROT_TEST_REQUIRE(context.runtime.request_load("missing_slot"));
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::pending);

            context.runtime.tick(0.f);

            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::failed);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().slot_id == "missing_slot");
        }

        void test_game_runtime_reports_capture_or_apply_hook_failures()
        {
            runtime_test_context_t context{ "runtime_hook_failures" };
            auto state{ std::make_unique<recording_save_state_t>(context.runtime) };
            recording_save_state_t* state_ptr{ state.get() };
            context.runtime.set_active_state(std::move(state));

            state_ptr->capture_should_succeed = false;
            CARROT_TEST_REQUIRE(context.runtime.request_save("manual_fail"));
            context.runtime.tick(0.f);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::failed);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().detail == "capture failed on purpose");

            state_ptr->capture_should_succeed = true;
            CARROT_TEST_REQUIRE(context.runtime.request_save("manual_ok"));
            context.runtime.tick(0.f);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::succeeded);

            state_ptr->apply_should_succeed = false;
            CARROT_TEST_REQUIRE(context.runtime.request_load("manual_ok"));
            context.runtime.tick(0.f);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::failed);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().detail == "apply failed on purpose");
        }

        void test_game_runtime_restores_engine_owned_sections()
        {
            runtime_test_context_t context{ "runtime_engine_sections" };
            auto state{ std::make_unique<recording_save_state_t>(context.runtime) };
            context.runtime.set_active_state(std::move(state));

            context.runtime.set_map_collision_debug_visible(true);
            context.runtime.set_object_collider_debug_visible(false);
            context.runtime.set_trigger_volume_debug_visible(true);

            CARROT_TEST_REQUIRE(context.runtime.request_save("engine_flags"));
            context.runtime.tick(0.f);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::succeeded);

            context.runtime.set_map_collision_debug_visible(false);
            context.runtime.set_object_collider_debug_visible(true);
            context.runtime.set_trigger_volume_debug_visible(false);

            CARROT_TEST_REQUIRE(context.runtime.request_load("engine_flags"));
            context.runtime.tick(0.f);

            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::succeeded);
            CARROT_TEST_REQUIRE(context.runtime.map_collision_debug_visible());
            CARROT_TEST_REQUIRE(!context.runtime.object_collider_debug_visible());
            CARROT_TEST_REQUIRE(context.runtime.trigger_volume_debug_visible());
        }

        void test_game_runtime_distinguishes_manual_autosave_and_temp_flows()
        {
            runtime_test_context_t context{ "runtime_save_flow_kinds" };
            auto state{ std::make_unique<recording_save_state_t>(context.runtime) };
            context.runtime.set_active_state(std::move(state));

            CARROT_TEST_REQUIRE(context.runtime.request_manual_save("manual_001"));
            context.runtime.tick(0.f);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::succeeded);

            CARROT_TEST_REQUIRE(context.runtime.request_autosave());
            context.runtime.tick(0.f);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::succeeded);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().slot_id == "autosave");

            CARROT_TEST_REQUIRE(context.runtime.request_temp_save());
            context.runtime.tick(0.f);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::succeeded);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().slot_id == "continue");

            const std::vector<save::save_slot_summary_t> all_slots{ context.runtime.list_save_slots() };
            CARROT_TEST_REQUIRE(all_slots.size() == 3u);
            CARROT_TEST_REQUIRE(context.runtime.list_save_slots(save::save_slot_kind_t::manual).size() == 1u);
            CARROT_TEST_REQUIRE(context.runtime.list_save_slots(save::save_slot_kind_t::autosave).size() == 1u);
            CARROT_TEST_REQUIRE(context.runtime.list_save_slots(save::save_slot_kind_t::temp).size() == 1u);

            const std::optional<save::save_slot_summary_t> autosave{ context.runtime.most_recent_save_slot(save::save_slot_kind_t::autosave) };
            CARROT_TEST_REQUIRE(autosave.has_value());
            CARROT_TEST_REQUIRE(autosave->slot_id == "autosave");
            CARROT_TEST_REQUIRE(autosave->metadata.slot_kind == save::save_slot_kind_t::autosave);

            const std::optional<save::save_slot_summary_t> temp_save{ context.runtime.most_recent_save_slot(save::save_slot_kind_t::temp) };
            CARROT_TEST_REQUIRE(temp_save.has_value());
            CARROT_TEST_REQUIRE(temp_save->slot_id == "continue");
            CARROT_TEST_REQUIRE(temp_save->metadata.slot_kind == save::save_slot_kind_t::temp);

            CARROT_TEST_REQUIRE(context.runtime.request_load_most_recent(save::save_slot_kind_t::temp));
            context.runtime.tick(0.f);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::succeeded);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().slot_id == "continue");

            CARROT_TEST_REQUIRE(context.runtime.request_load_most_recent(save::save_slot_kind_t::autosave));
            context.runtime.tick(0.f);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().outcome == save::save_request_outcome_t::succeeded);
            CARROT_TEST_REQUIRE(context.runtime.save_operation_status().slot_id == "autosave");
        }

        void test_save_service_reports_corrupt_and_incompatible_slot_failures()
        {
            constexpr std::string_view k_root_name{ "corrupt_and_incompatible" };
            reset_temp_root(k_root_name);

            io::virtual_file_system_t vfs;
            const std::filesystem::path save_root{ temp_root(k_root_name) / "save" };
            vfs.mount("save", save_root, false);
            save::save_service_t service{ vfs };

            const save::save_operation_status_t save_result{
                service.write_slot(save::save_request_t{
                    .kind = save::save_request_kind_t::save_slot,
                    .slot_id = "manual_001",
                    .slot_kind = save::save_slot_kind_t::manual
                }, std::span<const save::save_payload_section_t>{})
            };
            CARROT_TEST_REQUIRE(save_result.outcome == save::save_request_outcome_t::succeeded);

            const std::filesystem::path slot_path{ save_root / "slots" / "manual_001" };
            const std::filesystem::path metadata_path{ slot_path / "metadata.json" };
            const std::filesystem::path payload_path{ slot_path / "payload.bin" };

            {
                const std::string incompatible_metadata{
                    "{\n"
                    "  \"slot_id\": \"manual_001\",\n"
                    "  \"slot_kind\": \"manual\",\n"
                    "  \"save_format_version\": 99,\n"
                    "  \"payload_format_version\": 1,\n"
                    "  \"build_compatibility\": \"carrot-dev\",\n"
                    "  \"payload_file\": \"payload.bin\"\n"
                    "}\n"
                };
                const std::span<const std::uint8_t> metadata_bytes{
                    reinterpret_cast<const std::uint8_t*>(incompatible_metadata.data()),
                    incompatible_metadata.size()
                };
                CARROT_TEST_REQUIRE(carrot::utils::file::write_binary_file(metadata_path, metadata_bytes));

                save::save_operation_status_t load_status;
                const std::optional<save::loaded_save_slot_t> loaded{ service.load_slot("manual_001", load_status) };
                CARROT_TEST_REQUIRE(!loaded.has_value());
                CARROT_TEST_REQUIRE(load_status.outcome == save::save_request_outcome_t::failed);
                CARROT_TEST_REQUIRE(load_status.failure_reason == save::save_failure_reason_t::incompatible);
            }

            {
                const std::string compatible_metadata{
                    "{\n"
                    "  \"slot_id\": \"manual_001\",\n"
                    "  \"slot_kind\": \"manual\",\n"
                    "  \"save_format_version\": 1,\n"
                    "  \"payload_format_version\": 1,\n"
                    "  \"build_compatibility\": \"carrot-dev\",\n"
                    "  \"payload_file\": \"payload.bin\"\n"
                    "}\n"
                };
                const std::span<const std::uint8_t> metadata_bytes{
                    reinterpret_cast<const std::uint8_t*>(compatible_metadata.data()),
                    compatible_metadata.size()
                };
                CARROT_TEST_REQUIRE(carrot::utils::file::write_binary_file(metadata_path, metadata_bytes));
                const std::array<std::uint8_t, 3> corrupt_payload{ 0x43u, 0x53u, 0x41u };
                CARROT_TEST_REQUIRE(carrot::utils::file::write_binary_file(payload_path, corrupt_payload));

                save::save_operation_status_t load_status;
                const std::optional<save::loaded_save_slot_t> loaded{ service.load_slot("manual_001", load_status) };
                CARROT_TEST_REQUIRE(!loaded.has_value());
                CARROT_TEST_REQUIRE(load_status.outcome == save::save_request_outcome_t::failed);
                CARROT_TEST_REQUIRE(load_status.failure_reason == save::save_failure_reason_t::corrupt);
            }

            std::error_code ec;
            std::filesystem::remove(payload_path, ec);
            save::save_operation_status_t missing_payload_status;
            const std::optional<save::loaded_save_slot_t> missing_payload{
                service.load_slot("manual_001", missing_payload_status)
            };
            CARROT_TEST_REQUIRE(!missing_payload.has_value());
            CARROT_TEST_REQUIRE(missing_payload_status.outcome == save::save_request_outcome_t::failed);
            CARROT_TEST_REQUIRE(missing_payload_status.failure_reason == save::save_failure_reason_t::missing);

            std::filesystem::remove_all(temp_root(k_root_name), ec);
        }

        void test_save_service_round_trips_payload_sections_with_diagnostics()
        {
            constexpr std::string_view k_root_name{ "payload_round_trip" };
            reset_temp_root(k_root_name);

            io::virtual_file_system_t vfs;
            const std::filesystem::path save_root{ temp_root(k_root_name) / "save" };
            vfs.mount("save", save_root, false);
            save::save_service_t service{ vfs };

            const std::string gameplay_payload{ "quest=clocktower;stage=2" };
            const std::array<std::uint8_t, 4> engine_payload{ 1u, 0u, 1u, 0u };
            const std::vector<save::save_payload_section_t> sections{
                save::save_payload_section_t{
                    .section_id = "engine_runtime",
                    .owner = save::save_section_owner_t::engine,
                    .bytes = std::vector<std::uint8_t>{ engine_payload.begin(), engine_payload.end() }
                },
                save::save_payload_section_t{
                    .section_id = "gameplay_state",
                    .owner = save::save_section_owner_t::gameplay,
                    .bytes = std::vector<std::uint8_t>{
                        reinterpret_cast<const std::uint8_t*>(gameplay_payload.data()),
                        reinterpret_cast<const std::uint8_t*>(gameplay_payload.data()) + gameplay_payload.size()
                    }
                }
            };

            const save::save_operation_status_t save_result{
                service.write_slot(save::save_request_t{
                    .kind = save::save_request_kind_t::save_slot,
                    .slot_id = "payload_001",
                    .slot_kind = save::save_slot_kind_t::manual
                }, sections)
            };
            CARROT_TEST_REQUIRE(save_result.outcome == save::save_request_outcome_t::succeeded);

            save::save_operation_status_t load_status;
            const std::optional<save::loaded_save_slot_t> loaded{ service.load_slot("payload_001", load_status) };
            CARROT_TEST_REQUIRE(load_status.outcome == save::save_request_outcome_t::succeeded);
            CARROT_TEST_REQUIRE(loaded.has_value());
            CARROT_TEST_REQUIRE(loaded->sections.size() == 2u);

            const save::save_payload_section_t* loaded_engine{ loaded->find_section("engine_runtime") };
            CARROT_TEST_REQUIRE(loaded_engine != nullptr);
            CARROT_TEST_REQUIRE(loaded_engine->owner == save::save_section_owner_t::engine);
            const std::vector<std::uint8_t> expected_engine_bytes{ engine_payload.begin(), engine_payload.end() };
            CARROT_TEST_REQUIRE(loaded_engine->bytes == expected_engine_bytes);

            const save::save_payload_section_t* loaded_gameplay{ loaded->find_section("gameplay_state") };
            CARROT_TEST_REQUIRE(loaded_gameplay != nullptr);
            CARROT_TEST_REQUIRE(loaded_gameplay->owner == save::save_section_owner_t::gameplay);
            const std::string loaded_gameplay_text{
                reinterpret_cast<const char*>(loaded_gameplay->bytes.data()),
                loaded_gameplay->bytes.size()
            };
            CARROT_TEST_REQUIRE(loaded_gameplay_text == gameplay_payload);

            const std::string section_summary{ loaded->describe_sections() };
            CARROT_TEST_REQUIRE(section_summary.find("engine:engine_runtime") != std::string::npos);
            CARROT_TEST_REQUIRE(section_summary.find("gameplay:gameplay_state") != std::string::npos);

            std::error_code ec;
            std::filesystem::remove_all(temp_root(k_root_name), ec);
        }
    } // namespace

    void register_save_service_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("save service lists slots after save request",
                           test_save_service_lists_slots_after_save_request);
        tests.emplace_back("game runtime processes queued save requests",
                           test_game_runtime_processes_queued_save_requests);
        tests.emplace_back("game runtime reports capture or apply hook failures",
                           test_game_runtime_reports_capture_or_apply_hook_failures);
        tests.emplace_back("game runtime restores engine owned sections",
                           test_game_runtime_restores_engine_owned_sections);
        tests.emplace_back("game runtime distinguishes manual autosave and temp flows",
                           test_game_runtime_distinguishes_manual_autosave_and_temp_flows);
        tests.emplace_back("save service reports corrupt and incompatible slot failures",
                           test_save_service_reports_corrupt_and_incompatible_slot_failures);
        tests.emplace_back("save service round trips payload sections with diagnostics",
                           test_save_service_round_trips_payload_sections_with_diagnostics);
    }
} // namespace carrot::tests
