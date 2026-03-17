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
     * @param path Filesystem path to the file to load (as UTF-8 string view).
     *             The path is used as-is (no resolution or prefix handling performed).
     *
     * @return A vector containing the complete file contents on success.
     *
     * @note This function **never returns an empty vector** — failure to open or read
     *       the file is treated as a fatal error and logged via `LOG_CORE_FATAL`.
     * @note Uses binary mode (`std::ios::binary`) to prevent newline conversions
     *       or other text-mode transformations.
     * @note The file size is determined using `std::ios::ate` / `tellg()`; very large
     *       files (> ~2–4 GB depending on platform) may fail or behave unexpectedly.
     *
     * @see load_file_to_string() for text-oriented loading
     * @see resolve_asset_path() / try_resolve_asset_path() when loading from virtual/asset paths
     */
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
} // namespace carrot::utils::file
