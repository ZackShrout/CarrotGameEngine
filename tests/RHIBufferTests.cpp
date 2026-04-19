//
// Created by Zack Shrout on 4/18/2026.
//

#include "TestCommon.h"

#include "RHI/Buffer.h"

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace carrot::tests {
    namespace {
        void test_buffer_usage_names_cover_compute_growth()
        {
            using namespace carrot::rhi;

            CARROT_TEST_REQUIRE(buffer_usage_to_string(buffer_usage_t::vertex) == "vertex");
            CARROT_TEST_REQUIRE(buffer_usage_to_string(buffer_usage_t::storage) == "storage");
            CARROT_TEST_REQUIRE(buffer_usage_to_string(buffer_usage_t::indirect) == "indirect");
            CARROT_TEST_REQUIRE(buffer_usage_to_string(buffer_usage_t::readback) == "readback");
        }

        void test_buffer_usage_memory_preferences_match_contract()
        {
            using namespace carrot::rhi;

            CARROT_TEST_REQUIRE(buffer_usage_prefers_upload_memory(buffer_usage_t::staging));
            CARROT_TEST_REQUIRE(!buffer_usage_prefers_upload_memory(buffer_usage_t::storage));
            CARROT_TEST_REQUIRE(!buffer_usage_prefers_upload_memory(buffer_usage_t::indirect));

            CARROT_TEST_REQUIRE(buffer_usage_prefers_readback_memory(buffer_usage_t::readback));
            CARROT_TEST_REQUIRE(!buffer_usage_prefers_readback_memory(buffer_usage_t::staging));
            CARROT_TEST_REQUIRE(!buffer_usage_prefers_readback_memory(buffer_usage_t::uniform));
        }
    } // namespace

    void register_rhi_buffer_tests(std::vector<std::pair<std::string_view, std::function<void()>>>& tests)
    {
        tests.emplace_back("rhi buffer usage names cover compute growth", test_buffer_usage_names_cover_compute_growth);
        tests.emplace_back("rhi buffer usage memory preferences match contract",
                           test_buffer_usage_memory_preferences_match_contract);
    }
} // namespace carrot::tests
