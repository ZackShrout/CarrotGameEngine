//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <vector>

namespace carrot::utils::file {
    /**
     * @brief Loads the entire contents of a binary file into a vector of bytes.
     *
     * Opens the file in binary mode, determines its size, reads all bytes into a
     * `std::vector<std::uint8_t>`, and returns the content. This is the preferred
     * function for loading binary data (images, models, shaders, serialized data, etc.).
     *
     * @param path Filesystem path to the file to load.
     *             The path is used as-is (no resolution or prefix handling performed).
     *
     * @return A vector containing the complete file contents on success,
     *         or `std::nullopt` if the file cannot be opened, has an invalid size,
     *         or a read error occurs.
     *
     * @note Uses binary mode (`std::ios::binary`) to prevent newline conversions
     *       or other text-mode transformations.
     * @note The file size is determined using `std::ios::ate` / `tellg()`; very large
     *       files (> ~2–4 GB depending on platform) may fail or behave unexpectedly.
     * @note Errors are logged via the core logger, but no exceptions are thrown.
     *
     * @see load_file_to_string() for text-oriented loading
     */
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> load_binary_file(const std::filesystem::path& path) noexcept;

    /**
     * @brief Reads the entire contents of a file into a std::string.
     *
     * Loads the file using `load_binary_file()` and converts the resulting byte
     * buffer into a string. This is a convenience wrapper for text-oriented use cases.
     *
     * @param path Filesystem path to the file to load.
     *             The path is used as-is (no resolution or prefix handling performed).
     *
     * @return The complete file contents as a string on success,
     *         or `std::nullopt` if the file cannot be opened, has an invalid size,
     *         or a read error occurs.
     *
     * @note Uses binary mode via `load_binary_file()` to avoid newline conversions.
     * @note Suitable for both text and binary files; no encoding conversion is performed.
     * @note Errors are handled via return value; no exceptions are thrown.
     * @note May terminate the program on memory allocation failure.
     *
     * @see load_binary_file()
     */
    [[nodiscard]] std::optional<std::string> load_file_to_string(const std::filesystem::path& path) noexcept;

    /**
     * @brief Resolves an asset path string into a normalized filesystem path (as string_view).
     *
     * Converts virtual/relative/absolute asset path notations into a real filesystem path.
     * The returned string_view points into an internal cache (valid until engine shutdown
     * or explicit cache clear).
     *
     * Common supported patterns include:
     *  - absolute paths
     *  - relative paths (resolved against content root)
     *  - virtual prefixes like `res://`, `assets://`, `content://`, etc.
     *
     * @param path Asset path to resolve (UTF-8 string view).
     *
     * @return A non-owning view to the resolved path on success,
     *         or an empty `std::string_view` if resolution fails or the input is invalid/empty.
     *
     * @note The returned view is **not** guaranteed to point to an existing file.
     * @see try_resolve_asset_path() for a version that returns `std::filesystem::path` and `std::optional`.
     */
    [[nodiscard]] std::string_view resolve_asset_path(std::string_view path);

    /**
     * @brief Converts a filesystem path into a UTF-8 string suitable for logging.
     *
     * Produces a platform-consistent, human-readable string representation of a
     * `std::filesystem::path` for use with logging systems that expect narrow
     * character strings (e.g. `std::format` with `char`).
     *
     * On Windows, paths are internally stored as wide-character (UTF-16). This
     * function converts them to UTF-8 to ensure compatibility with the logger.
     * On POSIX systems (Linux/macOS), paths are typically already UTF-8 and are
     * returned directly.
     *
     * @param path Filesystem path to convert.
     *
     * @return A UTF-8 encoded string representation of the path.
     *
     * @note This function is intended for logging and debugging purposes only.
     *       It should not be used for filesystem operations that require native
     *       encoding or exact round-trip fidelity.
     * @note The returned string is safe to pass directly into `std::format` and
     *       other narrow-character APIs.
     *
     * @see std::filesystem::path::string()
     * @see std::filesystem::path::u8string()
     */
    [[nodiscard]] std::string to_log_string(const std::filesystem::path& path);
} // namespace carrot::utils::file
