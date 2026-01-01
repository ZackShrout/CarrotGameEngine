//
// Created by zshrout on 12/31/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include <csignal>

namespace carrot::utils {
    [[maybe_unused]] inline void debug_trap()
    {
#ifdef __clang__
#if __has_builtin(__builtin_debugtrap)
        __builtin_debugtrap();
#else
        __builtin_trap(); // Fallback, but less ideal
#endif
#elif defined(_MSC_VER)
        __debugbreak();
#elif defined(SIGTRAP)
        raise(SIGTRAP);
#else
        __builtin_trap(); // GCC/Clang fallback (aborts, but works)
#endif
    }

    // Conditional break — super useful in hot paths
    inline void debug_trap_if(const bool condition)
    {
        if (condition) [[unlikely]] debug_trap();
    }

    // Intent-based variants
    [[noreturn]] inline void unreachable()
    {
        // For code paths that should never be hit
#if defined(_MSC_VER)
        __assume(false);
#elif defined(__clang__) || defined(__GNUC__)
        __builtin_unreachable();
#else
        debug_trap();
#endif
    }
}

#ifdef _DEBUG
#define CE_BREAK()              carrot::utils::debug_trap()
#define CE_BREAK_IF(cond)       carrot::utils::debug_trap_if(cond)
#define CE_UNREACHABLE()        carrot::utils::unreachable()
#define CE_ASSUME(cond)         do { if (!(cond)) UNREACHABLE(); } while(0)
#else
#define CE_BREAK()              ((void)0)
#define CE_BREAK_IF(cond)       ((void)0)
#define CE_UNREACHABLE()        ((void)0)
#define CE_ASSUME(cond)         ((void)(cond))  // Let compiler optimize
#endif
