//
// Created by zshrout on 4/7/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TextEncoding.h"

#ifdef CARROT_PLATFORM_WIN32
#include <Windows.h>
#endif

namespace carrot::utils::text {
    std::wstring utf8_to_wide(const std::string_view utf8) noexcept
    {
#ifdef CARROT_PLATFORM_WIN32
        if (utf8.empty())
            return { };

        const int required_chars{
            MultiByteToWideChar(CP_UTF8,
                                MB_ERR_INVALID_CHARS,
                                utf8.data(),
                                static_cast<int>(utf8.size()),
                                nullptr,
                                0)
        };
        if (required_chars <= 0)
            return { };

        std::wstring wide(static_cast<size_t>(required_chars), L'\0');
        const int written{
            MultiByteToWideChar(CP_UTF8,
                                MB_ERR_INVALID_CHARS,
                                utf8.data(),
                                static_cast<int>(utf8.size()),
                                wide.data(),
                                required_chars)
        };
        if (written <= 0)
            return { };

        return wide;
#else
        return std::wstring(utf8.begin(), utf8.end());
#endif
    }

    std::string wide_to_utf8(const std::wstring_view wide) noexcept
    {
#ifdef CARROT_PLATFORM_WIN32
        if (wide.empty())
            return { };

        const int required_chars{
            WideCharToMultiByte(CP_UTF8,
                                WC_ERR_INVALID_CHARS,
                                wide.data(),
                                static_cast<int>(wide.size()),
                                nullptr,
                                0,
                                nullptr,
                                nullptr)
        };
        if (required_chars <= 0)
            return { };

        std::string utf8(static_cast<size_t>(required_chars), '\0');
        const int written{
            WideCharToMultiByte(CP_UTF8,
                                WC_ERR_INVALID_CHARS,
                                wide.data(),
                                static_cast<int>(wide.size()),
                                utf8.data(),
                                required_chars,
                                nullptr,
                                nullptr)
        };
        if (written <= 0)
            return { };

        return utf8;
#else
        return std::string(wide.begin(), wide.end());
#endif
    }
} // namespace carrot::utils::text

