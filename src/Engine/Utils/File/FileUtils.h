//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdio>
#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/types.h>
#endif

namespace carrot::utils::file {
#if defined(_WIN32)
    using file_offset_t = std::int64_t;
#else
    using file_offset_t = off_t;
#endif

    /**
     * @brief Opens a C file handle using the native platform-safe API.
     *
     * On Windows this uses `fopen_s()` to avoid CRT deprecation warnings.
     * On other platforms it uses `std::fopen()`.
     *
     * @param path Null-terminated filesystem path to open.
     * @param mode Null-terminated C file mode string (for example `"rb"` or `"wb"`).
     *
     * @return A valid `FILE*` on success, or `nullptr` if the file could not be opened.
     *
     * @note This is intended for low-level streaming/decoder code that already uses
     *       C file APIs. Higher-level file loading should generally prefer
     *       `load_binary_file()` or standard C++ streams.
     * @note This function does not log failures; callers are expected to handle and
     *       report errors in their own domain-specific context.
     *
     * @see seek_file()
     * @see tell_file()
     */
    [[nodiscard]] FILE* open_file(const char* path, const char* mode) noexcept;

    /**
     * @brief Seeks within an open C file handle using a 64-bit-capable native API.
     *
     * On Windows this uses `_fseeki64()`. On POSIX platforms it uses `fseeko()`.
     *
     * @param file Open file handle to seek within.
     * @param offset Byte offset to apply.
     * @param origin Seek origin constant such as `SEEK_SET`, `SEEK_CUR`, or `SEEK_END`.
     *
     * @return `0` on success, or a non-zero value on failure.
     *
     * @note This wrapper exists so platform-specific large-file seek APIs are hidden
     *       behind a single shared interface.
     *
     * @see open_file()
     * @see tell_file()
     */
    int seek_file(FILE* file, file_offset_t offset, int origin) noexcept;

    /**
     * @brief Returns the current file position using a 64-bit-capable native API.
     *
     * On Windows this uses `_ftelli64()`. On POSIX platforms it uses `ftello()`.
     *
     * @param file Open file handle to query.
     *
     * @return The current byte offset in the file, or a negative value on failure.
     *
     * @note This wrapper is paired with `seek_file()` so streaming code can use a
     *       single file-position type across platforms.
     *
     * @see open_file()
     * @see seek_file()
     */
    [[nodiscard]] file_offset_t tell_file(FILE* file) noexcept;

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
