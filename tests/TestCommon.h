//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <exception>
#include <sstream>
#include <string>

namespace carrot::tests {
    struct test_failure_t final : public std::exception
    {
        explicit test_failure_t(std::string message) noexcept
            : _message{ std::move(message) } {}

        [[nodiscard]] const char* what() const noexcept override
        {
            return _message.c_str();
        }

    private:
        std::string _message;
    };

    inline void require_impl(const bool condition,
                             const char* expression,
                             const char* file,
                             const int line,
                             const std::string& detail = {})
    {
        if (condition)
            return;

        std::ostringstream stream;
        stream << file << ":" << line << ": requirement failed: " << expression;
        if (!detail.empty())
            stream << " (" << detail << ")";

        throw test_failure_t{ stream.str() };
    }
} // namespace carrot::tests

#define CARROT_TEST_REQUIRE(expr) ::carrot::tests::require_impl((expr), #expr, __FILE__, __LINE__)
#define CARROT_TEST_REQUIRE_MSG(expr, msg) ::carrot::tests::require_impl((expr), #expr, __FILE__, __LINE__, (msg))
