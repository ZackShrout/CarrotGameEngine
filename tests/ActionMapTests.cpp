#include "TestCommon.h"

#include "Input/ActionMap.h"
#include "IO/VirtualFileSystem.h"

#include <filesystem>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        [[nodiscard]] std::filesystem::path game_assets_root()
        {
            return std::filesystem::path{ CARROT_SOURCE_ROOT } / "src" / "Game" / "assets";
        }

        void test_action_map_tracks_pressed_state()
        {
            input::input_action_map_t actions;
            actions.bind("move_up", input::key_code::w);

            const events::key_event_t press{
                ._key = input::key_code::w,
                ._action = events::key_action::press
            };
            actions.handle_key_event(press);
            CARROT_TEST_REQUIRE(actions.is_pressed("move_up"));

            const events::key_event_t release{
                ._key = input::key_code::w,
                ._action = events::key_action::release
            };
            actions.handle_key_event(release);
            CARROT_TEST_REQUIRE(!actions.is_pressed("move_up"));
        }

        void test_action_map_matches_modifier_binding()
        {
            input::input_action_map_t actions;
            actions.bind("toggle_fullscreen",
                         input::key_code::enter,
                         static_cast<uint8_t>(input::modifier::alt));

            const events::key_event_t enter_without_alt{
                ._key = input::key_code::enter,
                ._action = events::key_action::press,
                ._mods = 0
            };
            CARROT_TEST_REQUIRE(!actions.matches("toggle_fullscreen", enter_without_alt));

            const events::key_event_t enter_with_alt{
                ._key = input::key_code::enter,
                ._action = events::key_action::press,
                ._mods = static_cast<uint8_t>(input::modifier::alt)
            };
            CARROT_TEST_REQUIRE(actions.matches("toggle_fullscreen", enter_with_alt));
        }

        void test_action_map_loads_bindings_from_memory()
        {
            constexpr const char* json{
                R"({
                  "bindings": [
                    { "action": "move_up", "key": "W" },
                    { "action": "toggle_fullscreen", "key": "Enter", "mods": ["Alt"] }
                  ]
                })"
            };

            input::input_action_map_t actions;
            CARROT_TEST_REQUIRE(actions.load_bindings_from_memory(json, std::strlen(json)));

            const events::key_event_t move_up{
                ._key = input::key_code::w,
                ._action = events::key_action::press
            };
            CARROT_TEST_REQUIRE(actions.matches("move_up", move_up));

            const events::key_event_t fullscreen{
                ._key = input::key_code::enter,
                ._action = events::key_action::press,
                ._mods = static_cast<uint8_t>(input::modifier::alt)
            };
            CARROT_TEST_REQUIRE(actions.matches("toggle_fullscreen", fullscreen));
        }

        void test_action_map_rejects_invalid_memory_config()
        {
            constexpr const char* json{
                R"({
                  "bindings": [
                    { "action": "move_up", "key": "DefinitelyNotAKey" }
                  ]
                })"
            };

            input::input_action_map_t actions;
            actions.bind("move_up", input::key_code::w);
            CARROT_TEST_REQUIRE(!actions.load_bindings_from_memory(json, std::strlen(json)));

            const events::key_event_t move_up{
                ._key = input::key_code::w,
                ._action = events::key_action::press
            };
            CARROT_TEST_REQUIRE(actions.matches("move_up", move_up));
        }

        void test_action_map_loads_bindings_from_vfs_config()
        {
            io::virtual_file_system_t vfs;
            vfs.mount("game", game_assets_root(), true);

            input::input_action_map_t actions;
            CARROT_TEST_REQUIRE(actions.load_bindings_from_file(vfs, "game://config/input_actions.json"));

            const events::key_event_t move_right{
                ._key = input::key_code::right,
                ._action = events::key_action::press
            };
            CARROT_TEST_REQUIRE(actions.matches("move_right", move_right));

            const events::key_event_t fullscreen{
                ._key = input::key_code::enter,
                ._action = events::key_action::press,
                ._mods = static_cast<uint8_t>(input::modifier::alt)
            };
            CARROT_TEST_REQUIRE(actions.matches("toggle_fullscreen", fullscreen));
        }
    } // namespace

    void register_action_map_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("action map tracks pressed state", test_action_map_tracks_pressed_state);
        tests.emplace_back("action map matches modifier binding", test_action_map_matches_modifier_binding);
        tests.emplace_back("action map loads bindings from memory", test_action_map_loads_bindings_from_memory);
        tests.emplace_back("action map rejects invalid memory config", test_action_map_rejects_invalid_memory_config);
        tests.emplace_back("action map loads bindings from vfs config", test_action_map_loads_bindings_from_vfs_config);
    }
} // namespace carrot::tests
