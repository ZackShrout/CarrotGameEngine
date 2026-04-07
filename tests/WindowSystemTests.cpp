//
// Created by Zack Shrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "RHI/RHI.h"
#include "Window/Window.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        enum class test_runtime_window_role_t : uint8_t
        {
            gameplay_main = 0,
            gameplay_mirror = 1,
            log_console = 2,
        };

        struct test_runtime_window_spec_t
        {
            test_runtime_window_role_t role{ test_runtime_window_role_t::gameplay_main };
            window::window_create_desc_t create_desc{ };
            bool is_main_window{ false };
            bool register_for_presentation{ false };
            uint32_t presentation_channel_mask{ rhi::presentation_channel_gameplay };
            bool receives_gameplay_input{ false };
        };

        [[nodiscard]] std::vector<test_runtime_window_spec_t> build_test_runtime_window_specs(const uint32_t width,
                                                                                              const uint32_t height)
        {
            return {
                test_runtime_window_spec_t{
                    .role = test_runtime_window_role_t::gameplay_main,
                    .create_desc = {
                        .width = width,
                        .height = height,
                        .title = "Carrot Engine – Main Window"
                    },
                    .is_main_window = true,
                    .register_for_presentation = false,
                    .receives_gameplay_input = true
                },
                test_runtime_window_spec_t{
                    .role = test_runtime_window_role_t::gameplay_mirror,
                    .create_desc = { },
                    .is_main_window = false,
                    .register_for_presentation = true,
                    .presentation_channel_mask = rhi::presentation_channel_gameplay,
                    .receives_gameplay_input = false
                },
                test_runtime_window_spec_t{
                    .role = test_runtime_window_role_t::log_console,
                    .create_desc = {
                        .width = 720,
                        .height = 280,
                        .title = "Carrot Log Console"
                    },
                    .is_main_window = false,
                    .register_for_presentation = true,
                    .presentation_channel_mask = rhi::presentation_channel_log_console,
                    .receives_gameplay_input = false
                }
            };
        }

        void test_window_specs_include_expected_roles()
        {
            const auto specs{ build_test_runtime_window_specs(1280u, 720u) };
            CARROT_TEST_REQUIRE(specs.size() == 3u);

            CARROT_TEST_REQUIRE(specs[0].role == test_runtime_window_role_t::gameplay_main);
            CARROT_TEST_REQUIRE(specs[1].role == test_runtime_window_role_t::gameplay_mirror);
            CARROT_TEST_REQUIRE(specs[2].role == test_runtime_window_role_t::log_console);
        }

        void test_window_specs_have_exactly_one_main_window()
        {
            const auto specs{ build_test_runtime_window_specs(1280u, 720u) };

            const auto main_count{
                static_cast<size_t>(std::count_if(specs.begin(),
                                                  specs.end(),
                                                  [](const test_runtime_window_spec_t& spec)
                                                  {
                                                      return spec.is_main_window;
                                                  }))
            };

            CARROT_TEST_REQUIRE(main_count == 1u);

            const auto main_it{
                std::find_if(specs.begin(),
                             specs.end(),
                             [](const test_runtime_window_spec_t& spec)
                             {
                                 return spec.is_main_window;
                             })
            };

            CARROT_TEST_REQUIRE(main_it != specs.end());
            CARROT_TEST_REQUIRE(main_it->role == test_runtime_window_role_t::gameplay_main);
            CARROT_TEST_REQUIRE(main_it->receives_gameplay_input);
            CARROT_TEST_REQUIRE(!main_it->register_for_presentation);
        }

        void test_only_gameplay_main_receives_gameplay_input()
        {
            const auto specs{ build_test_runtime_window_specs(1280u, 720u) };

            const auto gameplay_input_count{
                static_cast<size_t>(std::count_if(specs.begin(),
                                                  specs.end(),
                                                  [](const test_runtime_window_spec_t& spec)
                                                  {
                                                      return spec.receives_gameplay_input;
                                                  }))
            };

            CARROT_TEST_REQUIRE(gameplay_input_count == 1u);

            for (const test_runtime_window_spec_t& spec : specs)
            {
                if (spec.role == test_runtime_window_role_t::gameplay_main)
                {
                    CARROT_TEST_REQUIRE(spec.receives_gameplay_input);
                }
                else
                {
                    CARROT_TEST_REQUIRE(!spec.receives_gameplay_input);
                }
            }
        }

        void test_auxiliary_windows_register_for_presentation()
        {
            const auto specs{ build_test_runtime_window_specs(1280u, 720u) };

            for (const test_runtime_window_spec_t& spec : specs)
            {
                if (spec.role == test_runtime_window_role_t::gameplay_main)
                {
                    CARROT_TEST_REQUIRE(!spec.register_for_presentation);
                }
                else
                {
                    CARROT_TEST_REQUIRE(spec.register_for_presentation);
                }
            }
        }

        void test_auxiliary_windows_use_expected_presentation_channels()
        {
            const auto specs{ build_test_runtime_window_specs(1280u, 720u) };

            const auto mirror_it{
                std::find_if(specs.begin(),
                             specs.end(),
                             [](const test_runtime_window_spec_t& spec)
                             {
                                 return spec.role == test_runtime_window_role_t::gameplay_mirror;
                             })
            };
            CARROT_TEST_REQUIRE(mirror_it != specs.end());
            CARROT_TEST_REQUIRE(mirror_it->presentation_channel_mask == rhi::presentation_channel_gameplay);

            const auto log_it{
                std::find_if(specs.begin(),
                             specs.end(),
                             [](const test_runtime_window_spec_t& spec)
                             {
                                 return spec.role == test_runtime_window_role_t::log_console;
                             })
            };
            CARROT_TEST_REQUIRE(log_it != specs.end());
            CARROT_TEST_REQUIRE(log_it->presentation_channel_mask == rhi::presentation_channel_log_console);
        }

        void test_log_console_window_defaults_match_expected_shape()
        {
            const auto specs{ build_test_runtime_window_specs(1280u, 720u) };

            const auto log_it{
                std::find_if(specs.begin(),
                             specs.end(),
                             [](const test_runtime_window_spec_t& spec)
                             {
                                 return spec.role == test_runtime_window_role_t::log_console;
                             })
            };

            CARROT_TEST_REQUIRE(log_it != specs.end());
            CARROT_TEST_REQUIRE(log_it->create_desc.width == 720u);
            CARROT_TEST_REQUIRE(log_it->create_desc.height == 280u);
            CARROT_TEST_REQUIRE(std::string_view{ log_it->create_desc.title } == "Carrot Log Console");
        }
    } // namespace

    void register_window_system_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("window specs include expected roles", test_window_specs_include_expected_roles);
        tests.emplace_back("window specs have exactly one main window", test_window_specs_have_exactly_one_main_window);
        tests.emplace_back("only gameplay main receives gameplay input", test_only_gameplay_main_receives_gameplay_input);
        tests.emplace_back("auxiliary windows register for presentation", test_auxiliary_windows_register_for_presentation);
        tests.emplace_back("auxiliary windows use expected presentation channels",
                           test_auxiliary_windows_use_expected_presentation_channels);
        tests.emplace_back("log console window defaults match expected shape",
                           test_log_console_window_defaults_match_expected_shape);
    }
} // namespace carrot::tests