//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <string>
#include <filesystem>
#include <optional>

namespace carrot::utils::file {
    /** */
    [[nodiscard]] std::vector<std::uint8_t> load_binary_file(std::string_view path);

    /**
     * @brief Reads the entire contents of a file into a std::string.
     *
     * Opens the file in binary mode, determines its size, reads all bytes into a string,
     * and returns the content. Handles common failure cases gracefully.
     *
     * @param path Filesystem path to the file to load.
     *             The path is used as-is (no resolution performed).
     *
     * @return The complete file contents as a string on success,
     *         or `std::nullopt` if the file cannot be opened, is not readable,
     *         has negative/invalid size, or a read error occurs.
     *
     * @note Uses binary mode (`std::ios::binary`) to avoid newline conversions.
     * @note Suitable for text and binary files (images, audio, shaders, etc.).
     */
    [[nodiscard]] std::optional<std::string> load_file_to_string(const std::filesystem::path& path);

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
     * @brief Attempts to resolve an asset path into a normalized filesystem::path.
     *
     * Similar to resolve_asset_path(), but returns an optional owning path object
     * and makes failure more explicit. Useful when you need to perform further
     * path operations or log the exact resolved path.
     *
     * @param path Asset path to resolve (UTF-8 string view).
     *
     * @return A normalized `std::filesystem::path` on successful resolution,
     *         or `std::nullopt` if the input is empty, invalid, or cannot be resolved.
     *
     * @note Does **not** check file existence — only performs path resolution logic.
     * @note Returned path uses the platform's preferred directory separators.
     */
    [[nodiscard]] std::optional<std::filesystem::path> try_resolve_asset_path(std::string_view path);

    /**
     * @brief Convenience wrapper: resolves path and returns filesystem::path (empty on failure).
     *
     * Equivalent to:
     * ```cpp
     * return try_resolve_asset_path(path).value_or(std::filesystem::path{});
     * ```
     *
     * @param path Asset path to resolve.
     *
     * @return Resolved path (with preferred separators), or an empty path object on failure.
     *
     * @note Prefer `try_resolve_asset_path()` when you need to distinguish failure from success.
     */
    inline std::filesystem::path resolve_asset_path_fs(std::string_view path)
    {
        return try_resolve_asset_path(path).value_or(std::filesystem::path{ });
    }
} // namespace carrot::utils::file
