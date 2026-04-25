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
    void register_asset_cooked_pipeline_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_asset_iteration_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_controller_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_collision_world_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_font_asset_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_forward_plus_data_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_json_writer_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_rhi_buffer_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_rhi_compute_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_save_service_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_scene_loading_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_tiled_world_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_ui_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_ui_layout_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_ui_navigation_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_window_system_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
    void register_world_composition_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests);
}

int main(const int argc, char** argv)
{
    using test_case_t = std::pair<std::string_view, std::function<void()>>;

    std::vector<test_case_t> tests;
    carrot::tests::register_action_map_tests(tests);
    carrot::tests::register_asset_cooked_pipeline_tests(tests);
    carrot::tests::register_asset_iteration_tests(tests);
    carrot::tests::register_controller_tests(tests);
    carrot::tests::register_collision_world_tests(tests);
    carrot::tests::register_font_asset_tests(tests);
    carrot::tests::register_forward_plus_data_tests(tests);
    carrot::tests::register_json_writer_tests(tests);
    carrot::tests::register_rhi_buffer_tests(tests);
    carrot::tests::register_rhi_compute_tests(tests);
    carrot::tests::register_save_service_tests(tests);
    carrot::tests::register_scene_loading_tests(tests);
    carrot::tests::register_tiled_world_tests(tests);
    carrot::tests::register_ui_tests(tests);
    carrot::tests::register_ui_layout_tests(tests);
    carrot::tests::register_ui_navigation_tests(tests);
    carrot::tests::register_window_system_tests(tests);
    carrot::tests::register_world_composition_tests(tests);

    const std::string_view filter{ argc > 1 && argv[1] ? std::string_view{ argv[1] } : std::string_view{} };
    size_t passed{ 0 };
    size_t selected{ 0 };

    for (const auto& [name, test] : tests)
    {
        if (!filter.empty() && name.find(filter) == std::string_view::npos)
            continue;

        ++selected;
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

    if (!filter.empty() && selected == 0u)
    {
        std::cerr << "[FAIL] No tests matched filter '" << filter << "'\n";
        return 1;
    }

    std::cout << "Passed " << passed << " test(s)\n";
    return 0;
}
