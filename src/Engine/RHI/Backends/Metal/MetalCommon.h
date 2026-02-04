//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Common/CommonHeaders.h"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>
#include <QuartzCore/CAMetalLayer.h>
#include <AppKit/AppKit.hpp>

// ──────────────────────────────────────────────────────────────────────────────
// MTL_CHECK          - non-fatal, logs error but continues (no error ptr)
// MTL_CHECK_FATAL    - fatal in debug (logs + CE_ASSERT), continues in release
// MTL_CHECKED        - non-fatal with error ptr details
// MTL_CHECKED_FATAL  - fatal in debug with error ptr details
// ──────────────────────────────────────────────────────────────────────────────

#ifdef _DEBUG

#define MTL_CHECK(call) ({ \
    auto __res__ = (call); \
    if (!__res__) { \
        carrot::core::logger_t::log( \
            std::source_location::current(), \
            carrot::core::log_category::graphics, \
            carrot::core::log_severity::error, \
            "Metal call failed (no error details available): {}", #call \
        ); \
    } \
    __res__; \
})

#define MTL_CHECK_FATAL(call) ({ \
    auto __res__ = (call); \
    if (!__res__) { \
        carrot::core::logger_t::log( \
            std::source_location::current(), \
            carrot::core::log_category::graphics, \
            carrot::core::log_severity::fatal, \
            "Metal fatal failure (no error details available): {}", #call \
        ); \
        CE_ASSERT(false && "Metal fatal error"); \
    } \
    __res__; \
})

#define MTL_CHECKED(call, err_ptr) ({ \
    auto __res__ = (call); \
    if (!__res__) { \
        std::string __msg__ = "Metal call failed: " #call; \
        if ((err_ptr) && *(err_ptr)) { \
            __msg__ += "\n  Error: "; \
            __msg__ += (*(err_ptr))->localizedDescription()->utf8String(); \
        } else { \
            __msg__ += "\n  (no detailed error available)"; \
        } \
        carrot::core::logger_t::log( \
            std::source_location::current(), \
            carrot::core::log_category::graphics, \
            carrot::core::log_severity::error, \
            "{}", __msg__.c_str() \
        ); \
    } \
    __res__; \
})

#define MTL_CHECKED_FATAL(call, err_ptr) ({ \
    auto __res__ = (call); \
    if (!__res__) { \
        std::string __msg__ = "Metal fatal failure: " #call; \
        if ((err_ptr) && *(err_ptr)) { \
            __msg__ += "\n  Error: "; \
            __msg__ += (*(err_ptr))->localizedDescription()->utf8String(); \
        } else { \
            __msg__ += "\n  (no detailed error available)"; \
        } \
        carrot::core::logger_t::log( \
            std::source_location::current(), \
            carrot::core::log_category::graphics, \
            carrot::core::log_severity::fatal, \
            "{}", __msg__.c_str() \
        ); \
        CE_ASSERT(false && "Metal fatal error"); \
    } \
    __res__; \
})

#else

#define MTL_CHECK(call)                 (call)
#define MTL_CHECK_FATAL(call)           (call)
#define MTL_CHECKED(call, err_ptr)      (call)
#define MTL_CHECKED_FATAL(call, err_ptr)(call)

#endif