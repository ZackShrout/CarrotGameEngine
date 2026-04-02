#include "TestCommon.h"

#include "Input/ActionMap.h"

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
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
    } // namespace

    void register_action_map_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("action map tracks pressed state", test_action_map_tracks_pressed_state);
        tests.emplace_back("action map matches modifier binding", test_action_map_matches_modifier_binding);
    }
} // namespace carrot::tests
