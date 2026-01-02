//
// Created by zshrout on 12/31/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include "DebugBreak.h"
#include "Engine/Core/Logger.h"

namespace carrot {
#ifdef _DEBUG

#define CE_ASSERT(cond, ...)                                                            \
        do {                                                                            \
                if (!(cond)) {                                                          \
                        carrot::core::logger_t::log(std::source_location::current(),    \
                        carrot::core::log_category::core,                               \
                        carrot::core::log_severity::fatal,                              \
                        "Assertion failed: " #cond " -- " __VA_ARGS__);                 \
                        CE_BREAK();                                                     \
                }                                                                       \
        } while (0)

        // Soft ensure - logs but continues (no break)
#define CE_ENSURE(cond, ...)                                                            \
        do {                                                                            \
                if (!(cond)) {                                                          \
                        carrot::core::logger_t::log(std::source_location::current(),    \
                        carrot::core::log_category::core,                               \
                        carrot::core::log_severity::error,                              \
                        "Ensure failed: " #cond " -- " __VA_ARGS__);                    \
                }                                                                       \
        } while (0)

#else

#define CE_ASSERT(cond, ...)    ((void)0)
#define CE_ENSURE(cond, ...)    ((void)(cond))  // Still evaluate in case of side effects

#endif

        // Always-active variants (rare, but useful)
#define CE_ASSERT_ALWAYS(cond, ...)                                                     \
        do {                                                                            \
                if (!(cond)) {                                                          \
                        carrot::core::logger_t::log(carrot::core::log_category::core,   \
                        carrot::core::logger_t::log(std::source_location::current(),    \
                        carrot::core::log_category::core,                               \
                        carrot::core::log_severity::fatal,                              \
                        "Assertion (always) failed: " #cond " -- " __VA_ARGS__);        \
                        CE_BREAK();                                                     \
                }                                                                       \
        } while (0)
} // namespace carrot
