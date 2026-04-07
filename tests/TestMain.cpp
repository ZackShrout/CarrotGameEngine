//
// Created by Zack Shrout on 4/6/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include <functional>
#include <iostream>
#include <string_view>
#include <vector>

namespace carrot::tests {
    void register_action_map_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_controller_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_collision_world_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_scene_loading_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_window_system_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
}

int main()
{
    using test_case_t = std::pair<std::string_view, std::function<void()>>;

    std::vector<test_case_t> tests;
    carrot::tests::register_action_map_tests(tests);
    carrot::tests::register_controller_tests(tests);
    carrot::tests::register_collision_world_tests(tests);
    carrot::tests::register_scene_loading_tests(tests);
    carrot::tests::register_window_system_tests(tests);

    size_t passed{ 0 };

    for (const auto& [name, test] : tests)
    {
        try
        {
            test();
            std::cout << "[PASS] " << name << "\n";
            ++passed;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[FAIL] " << name << ": " << e.what() << "\n";
            return 1;
        }
    }

    std::cout << "Passed " << passed << " test(s)\n";
    return 0;
}
