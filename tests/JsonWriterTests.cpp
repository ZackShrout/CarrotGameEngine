//
// Created by Zack Shrout on 4/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestCommon.h"

#include "Utils/JSON/Public/JsonWriter.h"

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        void test_json_writer_pretty_object_and_array_output()
        {
            utils::json::json_writer_t writer;
            writer.begin_object();
            writer.key("name");
            writer.value("Carrot");
            writer.key("versions");
            writer.begin_array();
            writer.value(29u);
            writer.value(30u);
            writer.end_array();
            writer.key("enabled");
            writer.value(true);
            writer.end_object();

            CARROT_TEST_REQUIRE(writer.str() ==
                "{\n"
                "  \"name\": \"Carrot\",\n"
                "  \"versions\": [\n"
                "    29,\n"
                "    30\n"
                "  ],\n"
                "  \"enabled\": true\n"
                "}");
        }

        void test_json_writer_escapes_special_characters()
        {
            utils::json::json_writer_t writer;
            writer.begin_object();
            writer.key("message");
            writer.value("Quote: \"\nTab:\tSlash:\\");
            writer.end_object();

            CARROT_TEST_REQUIRE(writer.str() ==
                "{\n"
                "  \"message\": \"Quote: \\\"\\nTab:\\tSlash:\\\\\"\n"
                "}");
        }

        void test_json_writer_compact_mode_has_no_extra_whitespace()
        {
            utils::json::json_writer_t writer{ utils::json::json_writer_style_t::compact };
            writer.begin_object();
            writer.key("slot_id");
            writer.value("autosave");
            writer.key("payload_size_bytes");
            writer.value(12u);
            writer.key("valid");
            writer.value(true);
            writer.end_object();

            CARROT_TEST_REQUIRE(writer.str() ==
                                "{\"slot_id\":\"autosave\",\"payload_size_bytes\":12,\"valid\":true}");
        }

        void test_json_writer_take_resets_writer_state()
        {
            utils::json::json_writer_t writer;
            writer.begin_array();
            writer.value("a");
            writer.value("b");
            writer.end_array();

            const std::string output{ writer.take() };
            CARROT_TEST_REQUIRE(output ==
                                "[\n"
                                "  \"a\",\n"
                                "  \"b\"\n"
                                "]");
            CARROT_TEST_REQUIRE(writer.str().empty());

            writer.begin_object();
            writer.key("ok");
            writer.value(true);
            writer.end_object();
            CARROT_TEST_REQUIRE(writer.str() ==
                                "{\n"
                                "  \"ok\": true\n"
                                "}");
        }
    } // namespace

    void register_json_writer_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("json writer pretty object and array output",
                           test_json_writer_pretty_object_and_array_output);
        tests.emplace_back("json writer escapes special characters",
                           test_json_writer_escapes_special_characters);
        tests.emplace_back("json writer compact mode has no extra whitespace",
                           test_json_writer_compact_mode_has_no_extra_whitespace);
        tests.emplace_back("json writer take resets writer state",
                           test_json_writer_take_resets_writer_state);
    }
} // namespace carrot::tests
