//
// Created by zshrout on 12/30/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include "Common/CommonHeaders.h"

#include <vector>
#include <functional>

namespace carrot::utils {
    // Primary template - we only support void return types
    template<typename T>
    class single_delegate_t;

    template<typename... Args>
    class multicast_delegate_t;

    // Partial specialization for function type: void(Args...)
    template<typename... Args>
    class single_delegate_t<void(Args...)>
    {
    public:
        using stub_ptr = void(*)(void* object_ptr, Args... args);

        single_delegate_t() noexcept : _object_ptr(nullptr), _stub_ptr(nullptr) {}

        ~single_delegate_t()
        {
            if (deleter && _object_ptr)
                deleter(_object_ptr);
        }

        // Bind free/static function
        template<auto candidate>
        static single_delegate_t bind()
        {
            return {
                nullptr, [](void*, Args... args) {
                    std::invoke(candidate, std::forward<Args>(args)...);
                }
            };
        }

        // Bind member function
        template<auto candidate, typename T>
        static single_delegate_t bind(T* object)
        {
            return {
                object, [](void* obj, Args... args) {
                    std::invoke(candidate, static_cast<T *>(obj), std::forward<Args>(args)...);
                }
            };
        }

        // Bind functor/lambda (non-capturing or small capture)
        // Note: Large capturing lambdas need heap allocation or shared ownership
        template<typename Functor>
        static single_delegate_t bind(Functor&& functor)
        {
            static_assert(sizeof(Functor) <= sizeof(void *),
                          "Functor too large for inline storage - use pointer or shared_ptr");
            static auto* stored = new Functor(std::forward<Functor>(functor));
            return {
                stored, [](void* obj, Args... args) {
                    (*static_cast<Functor *>(obj))(std::forward<Args>(args)...);
                },
                [](void* obj) {
                    delete static_cast<Functor *>(obj);
                }
            };
        }

        void invoke(Args... args) const
        {
            CE_ASSERT(_stub_ptr, "Invoking empty delegate");
            _stub_ptr(_object_ptr, std::forward<Args>(args)...);
        }

        explicit operator bool() const noexcept { return _stub_ptr != nullptr; }
        [[nodiscard]] bool is_valid() const noexcept { return _stub_ptr != nullptr; }

        // For removal - expose raw pointers (careful!)
        [[nodiscard]] void* get_object_ptr() const noexcept { return _object_ptr; }
        [[nodiscard]] stub_ptr get_stub_ptr() const noexcept { return _stub_ptr; }

    private:
        friend class multicast_delegate_t<void(Args...)>;

        // Private constructor for internal use
        single_delegate_t(void* obj, const stub_ptr stub) : _object_ptr(obj), _stub_ptr(stub) {}

        single_delegate_t(void* obj, const stub_ptr stub, void (*del)(void*)) : _object_ptr(obj), _stub_ptr(stub),
            deleter(del) {}

        void* _object_ptr;
        stub_ptr _stub_ptr;

        void (*deleter)(void*){ nullptr };
    };

    // Multicast version - just a vector of single_delegate_t
    template<typename... Args>
    class multicast_delegate_t<void(Args...)>
    {
    public:
        using delegate_type = single_delegate_t<void(Args...)>;

        void add(delegate_type delegate) { _delegates.emplace_back(std::move(delegate)); }

        void remove(const delegate_type& delegate)
        {
            // NOTE: Simple removal - O(n), fine for small counts (typical for tick systems).
            //       For faster removal later: use handles (indices) or unordered_set with hash
            std::erase_if(_delegates, [&](const auto& d) {
                return d.get_object_ptr() == delegate.get_object_ptr() && d.get_stub_ptr() == delegate.get_stub_ptr();
            });
        }

        void broadcast(Args... args) const
        {
            for (const auto& del: _delegates)
            {
                if (del)
                    del.invoke(std::forward<Args>(args)...);
            }
        }

        void clear() { _delegates.clear(); }

        [[nodiscard]] size_t count() const noexcept { return _delegates.size(); }

    private:
        std::vector<delegate_type> _delegates;
    };
} // carrot::utils
