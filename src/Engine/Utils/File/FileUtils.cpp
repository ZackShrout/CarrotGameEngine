//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "FileUtils.h"
#include "Utils/TextEncoding.h"

#include <bit>

namespace carrot::utils::file {
    namespace {
    } // anonymous namespace

    FILE* open_file(const char* path, const char* mode) noexcept
    {
#ifdef _WIN32
        FILE* file{ nullptr };
        return (fopen_s(&file, path, mode) == 0) ? file : nullptr;
#else
        return std::fopen(path, mode);
#endif
    }

    int seek_file(FILE* file, const file_offset_t offset, const int origin) noexcept
    {
#ifdef _WIN32
        return _fseeki64(file, offset, origin);
#else
        return fseeko(file, offset, origin);
#endif
    }

    file_offset_t tell_file(FILE* file) noexcept
    {
#ifdef _WIN32
        return _ftelli64(file);
#else
        return ftello(file);
#endif
    }

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> load_binary_file(const std::filesystem::path& path) noexcept
    {
        std::ifstream file{ path, std::ios::binary | std::ios::ate };
        if (!file.is_open())
        {
            LOG_CORE_ERROR("Failed to open file '{}'", to_log_string(path));
            return std::nullopt;
        }

        const std::streampos file_size{ file.tellg() };
        if (file_size < 0)
        {
            LOG_CORE_ERROR("Invalid file size for '{}'", to_log_string(path));
            return std::nullopt;
        }

        std::vector<std::uint8_t> data(static_cast<size_t>(file_size));

        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char *>(data.data()), file_size))
        {
            LOG_CORE_ERROR("Failed to read file '{}'", to_log_string(path));
            return std::nullopt;
        }

        return data;
    }

    bool write_binary_file(const std::filesystem::path& path, const std::span<const std::uint8_t> data) noexcept
    {
        if (!path.parent_path().empty())
        {
            std::error_code create_error;
            std::filesystem::create_directories(path.parent_path(), create_error);
            if (create_error)
            {
                LOG_CORE_ERROR("Failed to create parent directories for '{}': {}",
                               to_log_string(path),
                               create_error.message());
                return false;
            }
        }

        std::ofstream file{ path, std::ios::binary | std::ios::trunc };
        if (!file.is_open())
        {
            LOG_CORE_ERROR("Failed to open file for writing '{}'", to_log_string(path));
            return false;
        }

        if (!data.empty())
        {
            file.write(reinterpret_cast<const char*>(data.data()),
                       static_cast<std::streamsize>(data.size()));
        }

        if (!file.good())
        {
            LOG_CORE_ERROR("Failed to write file '{}'", to_log_string(path));
            return false;
        }

        return true;
    }

    std::optional<std::string> load_file_to_string(const std::filesystem::path& path) noexcept
    {
        auto bytes = load_binary_file(path);
        if (!bytes) return std::nullopt;

        return std::string(bytes->begin(), bytes->end());
    }

    size_t binary_blob_writer_t::write_bytes(const std::span<const std::uint8_t> bytes)
    {
        const size_t offset{ _data.size() };
        _data.insert(_data.end(), bytes.begin(), bytes.end());
        return offset;
    }

    size_t binary_blob_writer_t::write_zeroes(const size_t byte_count)
    {
        const size_t offset{ _data.size() };
        _data.resize(offset + byte_count, 0u);
        return offset;
    }

    size_t binary_blob_writer_t::align(const size_t alignment, const std::uint8_t fill)
    {
        if (alignment == 0u)
            return _data.size();

        const size_t remainder{ _data.size() % alignment };
        if (remainder == 0u)
            return _data.size();

        const size_t padding{ alignment - remainder };
        _data.insert(_data.end(), padding, fill);
        return _data.size();
    }

    size_t binary_blob_writer_t::write_u8(const std::uint8_t value)
    {
        _data.push_back(value);
        return _data.size() - 1u;
    }

    size_t binary_blob_writer_t::write_u16(const std::uint16_t value)
    {
        return write_unsigned_le(value);
    }

    size_t binary_blob_writer_t::write_u32(const std::uint32_t value)
    {
        return write_unsigned_le(value);
    }

    size_t binary_blob_writer_t::write_u64(const std::uint64_t value)
    {
        return write_unsigned_le(value);
    }

    size_t binary_blob_writer_t::write_f32(const float value)
    {
        return write_u32(std::bit_cast<std::uint32_t>(value));
    }

    bool binary_blob_writer_t::patch_u32(const size_t offset, const std::uint32_t value) noexcept
    {
        return patch_unsigned_le(offset, value);
    }

    bool binary_blob_writer_t::patch_u64(const size_t offset, const std::uint64_t value) noexcept
    {
        return patch_unsigned_le(offset, value);
    }

    bool binary_blob_writer_t::patch_f32(const size_t offset, const float value) noexcept
    {
        return patch_u32(offset, std::bit_cast<std::uint32_t>(value));
    }

    std::string to_log_string(const std::filesystem::path& path)
    {
#ifdef _WIN32
        return utils::text::wide_to_utf8(path.native());
#else
        return path.string();
#endif
    }
} // namespace carrot::utils::file
