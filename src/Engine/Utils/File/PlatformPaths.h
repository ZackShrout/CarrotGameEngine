//
// Created by Zack Shrout on 3/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <filesystem>

namespace carrot::utils::file {
    /**
     * @brief Retrieves the absolute filesystem path to the currently running executable.
     *
     * Returns the full, normalized path to the executable binary for the current process.
     * This is a fundamental primitive used for resolving engine-relative paths such as
     * asset roots, configuration directories, and runtime data locations.
     *
     * Platform-specific implementations:
     *
     * - **Windows**:
     *   Uses `GetModuleFileNameW(nullptr, ...)` to retrieve the UTF-16 path of the
     *   current module (the executable), which is then converted into a
     *   `std::filesystem::path`.
     *
     * - **Linux**:
     *   Reads the `/proc/self/exe` symbolic link via `readlink()` to obtain the
     *   absolute path to the running executable.
     *
     * - **macOS**:
     *   Uses `_NSGetExecutablePath()` to retrieve the executable path, which may
     *   require normalization via `realpath()` to resolve symlinks.
     *
     * @return The absolute path to the current executable on success,
     *         or an empty path if the operation fails.
     *
     * @note This function does not guarantee that the returned path exists on disk
     *       (e.g. in restricted or sandboxed environments).
     * @note The returned path may contain symlinks depending on platform behavior;
     *       callers may normalize further if strict canonicalization is required.
     *
     * @see executable_directory()
     */
    [[nodiscard]] std::filesystem::path executable_path() noexcept;

    /**
     * @brief Retrieves the directory containing the currently running executable.
     *
     * Returns the parent directory of the path provided by `executable_path()`.
     * This is commonly used as a base for resolving engine-relative resources,
     * configuration files, and runtime data directories.
     *
     * @return The directory containing the executable on success,
     *         or an empty path if `executable_path()` fails.
     *
     * @note This is a convenience wrapper around `executable_path()`.
     * @note The returned path may contain symlinks depending on platform behavior.
     *
     * @see executable_path()
     */
    [[nodiscard]] inline std::filesystem::path executable_directory() noexcept
    {
        return executable_path().parent_path();
    }
} // namespace carrot::utils::file
