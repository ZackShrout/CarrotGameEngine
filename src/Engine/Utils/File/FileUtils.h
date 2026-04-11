//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdio>
#include <span>
#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
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
     * @brief Writes a complete binary blob to disk.
     *
     * Creates parent directories when needed, opens the file in binary truncation mode,
     * and writes the provided bytes exactly as given.
     *
     * @param path Destination filesystem path.
     * @param data Byte span to write.
     *
     * @return `true` on success, otherwise `false`.
     */
    [[nodiscard]] bool write_binary_file(const std::filesystem::path& path,
                                         std::span<const std::uint8_t> data) noexcept;

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
     * @brief Incrementally builds a contiguous binary blob in memory.
     *
     * Provides a small utility for writing serialized binary data into an owned
     * `std::vector<std::uint8_t>`. The writer supports appending raw bytes,
     * zero-padding, alignment, little-endian primitive emission, and in-place
     * patching of previously written fields.
     *
     * This is intended for cooked asset formats and other engine-side binary
     * serialization code where deterministic layout matters and random-access
     * patching of offsets or sizes is useful.
     *
     * @note Integer values are written in little-endian byte order regardless of
     *       host platform endianness.
     * @note The return value of each write method is the byte offset where that
     *       value or block begins within the blob.
     * @note This type only accumulates bytes in memory; it does not write to disk
     *       directly. Use `write_binary_file()` to persist the final blob.
     */
    class binary_blob_writer_t
    {
    public:
        /**
         * @brief Returns the current size of the blob in bytes.
         *
         * @return Number of bytes currently stored.
         */
        [[nodiscard]] size_t size() const noexcept { return _data.size(); }

        /**
         * @brief Returns whether the blob currently contains no bytes.
         *
         * @return `true` when `size() == 0`, otherwise `false`.
         */
        [[nodiscard]] bool empty() const noexcept { return _data.empty(); }

        /**
         * @brief Returns a read-only view of the accumulated binary data.
         *
         * @return A span over the current blob contents.
         *
         * @note The returned span is invalidated by any subsequent operation that
         *       resizes or reallocates the underlying storage.
         */
        [[nodiscard]] std::span<const std::uint8_t> data() const noexcept { return _data; }

        /**
         * @brief Clears all accumulated bytes while retaining allocated capacity.
         */
        void clear() noexcept { _data.clear(); }

        /**
         * @brief Reserves storage for at least the requested number of bytes.
         *
         * @param byte_count Minimum total capacity to reserve.
         *
         * @note This can reduce reallocations when the final blob size is known or
         *       can be estimated up front.
         */
        void reserve(const size_t byte_count) { _data.reserve(byte_count); }

        /**
         * @brief Appends a raw sequence of bytes to the blob.
         *
         * @param bytes Byte span to append.
         *
         * @return The starting byte offset of the appended block.
         */
        [[nodiscard]] size_t write_bytes(std::span<const std::uint8_t> bytes);

        /**
         * @brief Appends a run of zero-valued bytes to the blob.
         *
         * @param byte_count Number of zero bytes to append.
         *
         * @return The starting byte offset of the zero-filled block.
         */
        [[nodiscard]] size_t write_zeroes(size_t byte_count);

        /**
         * @brief Pads the blob to the next boundary of the requested alignment.
         *
         * Appends `fill` bytes until `size()` is evenly divisible by `alignment`.
         * If the blob is already aligned, no bytes are added.
         *
         * @param alignment Required byte alignment. Values less than or equal to
         *                  `1` result in no padding.
         * @param fill Byte value used for padding.
         *
         * @return The byte offset where padding began, or the current size if no
         *         padding was required.
         */
        [[nodiscard]] size_t align(size_t alignment, std::uint8_t fill = 0u);

        /**
         * @brief Writes an unsigned 8-bit integer.
         *
         * @param value Value to append.
         *
         * @return The starting byte offset of the written value.
         */
        [[nodiscard]] size_t write_u8(std::uint8_t value);

        /**
         * @brief Writes an unsigned 16-bit integer in little-endian order.
         *
         * @param value Value to append.
         *
         * @return The starting byte offset of the written value.
         */
        [[nodiscard]] size_t write_u16(std::uint16_t value);

        /**
         * @brief Writes an unsigned 32-bit integer in little-endian order.
         *
         * @param value Value to append.
         *
         * @return The starting byte offset of the written value.
         */
        [[nodiscard]] size_t write_u32(std::uint32_t value);

        /**
         * @brief Writes an unsigned 64-bit integer in little-endian order.
         *
         * @param value Value to append.
         *
         * @return The starting byte offset of the written value.
         */
        [[nodiscard]] size_t write_u64(std::uint64_t value);

        /**
         * @brief Writes a 32-bit floating-point value using its raw IEEE-754 bits.
         *
         * @param value Value to append.
         *
         * @return The starting byte offset of the written value.
         */
        [[nodiscard]] size_t write_f32(float value);

        /**
         * @brief Overwrites an existing 32-bit unsigned integer in-place.
         *
         * @param offset Byte offset at which the value begins.
         * @param value Replacement value to write in little-endian order.
         *
         * @return `true` on success, or `false` if the offset would extend past the
         *         end of the blob.
         */
        [[nodiscard]] bool patch_u32(size_t offset, std::uint32_t value) noexcept;

        /**
         * @brief Overwrites an existing 64-bit unsigned integer in-place.
         *
         * @param offset Byte offset at which the value begins.
         * @param value Replacement value to write in little-endian order.
         *
         * @return `true` on success, or `false` if the offset would extend past the
         *         end of the blob.
         */
        [[nodiscard]] bool patch_u64(size_t offset, std::uint64_t value) noexcept;

        /**
         * @brief Overwrites an existing 32-bit floating-point value in-place.
         *
         * @param offset Byte offset at which the value begins.
         * @param value Replacement value to write using its raw IEEE-754 bits.
         *
         * @return `true` on success, or `false` if the offset would extend past the
         *         end of the blob.
         */
        [[nodiscard]] bool patch_f32(size_t offset, float value) noexcept;

        /**
         * @brief Moves the owned byte vector out of the writer.
         *
         * @return The accumulated blob as a `std::vector<std::uint8_t>`.
         *
         * @note This method is rvalue-qualified to make transfer of ownership
         *       explicit at the call site.
         */
        [[nodiscard]] std::vector<std::uint8_t> take() && noexcept { return std::move(_data); }

    private:
        template<typename UInt>
        [[nodiscard]] size_t write_unsigned_le(UInt value)
        {
            static_assert(std::is_unsigned_v<UInt>, "write_unsigned_le requires an unsigned integer type");

            constexpr size_t byte_count{ sizeof(UInt) };
            const size_t offset{ _data.size() };
            _data.resize(offset + byte_count);

            for (size_t i{ 0 }; i < byte_count; ++i)
                _data[offset + i] = static_cast<std::uint8_t>((value >> (i * 8u)) & static_cast<UInt>(0xFFu));

            return offset;
        }

        template<typename UInt>
        [[nodiscard]] bool patch_unsigned_le(const size_t offset, UInt value) noexcept
        {
            static_assert(std::is_unsigned_v<UInt>, "patch_unsigned_le requires an unsigned integer type");

            constexpr size_t byte_count{ sizeof(UInt) };
            if (offset + byte_count > _data.size())
                return false;

            for (size_t i{ 0 }; i < byte_count; ++i)
                _data[offset + i] = static_cast<std::uint8_t>((value >> (i * 8u)) & static_cast<UInt>(0xFFu));

            return true;
        }

        std::vector<std::uint8_t> _data;
    };

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
