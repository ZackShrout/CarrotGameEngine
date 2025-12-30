//
// Created by zshrout on 12/29/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//
// Cross-platform debug printing and trapping macros
// - Active only when _DEBUG is defined
// - Zero overhead in release builds
// - Portable debug_trap() across Clang, GCC, MSVC
// - printf-style formatting everywhere

#pragma once

#include <cstdio>
#include <csignal>

#ifdef _DEBUG

[[maybe_unused]] static inline void debug_trap()
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

// General debug message (visible in console, no breakpoint)
#ifndef MESSAGE
#define MESSAGE(format, ...)        \
do {                                \
printf(format "\n", ##__VA_ARGS__); \
fflush(stdout);                     \
} while (0)
#endif // !MESSAGE

// Fatal error message + immediate debugger break
#ifndef ERROR_MSSG
#define ERROR_MSSG(format, ...)                         \
do {                                                    \
fprintf(stderr, "ERROR: " format "\n", ##__VA_ARGS__);  \
fflush(stderr);                                         \
debug_trap();                                           \
} while (0)
#endif // !ERROR_MSSG

// Optional: nicer formatted versions with file/line
#ifndef MESSAGE_FL
#define MESSAGE_FL(format, ...)                                     \
do {                                                                \
printf("%s:%d: " format "\n", __FILE__, __LINE__, ##__VA_ARGS__);   \
fflush(stdout);                                                     \
} while (0)
#endif // !MESSAGE_FL

#ifndef ERROR_MSSG_FL
#define ERROR_MSSG_FL(format, ...)                                                  \
do {                                                                                \
fprintf(stderr, "%s:%d: ERROR: " format "\n", __FILE__, __LINE__, ##__VA_ARGS__);   \
fflush(stderr);                                                                     \
debug_trap();                                                                       \
} while (0)
#endif // !ERROR_MSSG_FL

#ifndef ASSERT
#define ASSERT(condition, format, ...)                                          \
do {                                                                            \
if (!(condition)) {                                                             \
ERROR_MSSG_FL("Assertion failed: " #condition " -- " format, ##__VA_ARGS__);    \
}                                                                               \
} while (0)
#endif

#else

#ifndef MESSAGE
#define MESSAGE(...) do { } while (0)
#endif

#ifndef ERROR_MSSG
#define ERROR_MSSG(...) do { } while (0)
#endif

#ifndef MESSAGE_FL
#define MESSAGE_FL(...) do { } while (0)
#endif

#ifndef ERROR_MSSG_FL
#define ERROR_MSSG_FL(...) do { } while (0)
#endif

#ifndef ASSERT
#define ASSERT(condition, ...) do { } while (0)
#endif

#endif
