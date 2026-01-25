//
// Created by zshrout on 12/30/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#pragma once

#include "Common/CommonHeaders.h"

#include <vector>
#include <functional>

namespace carrot::utils {
    template<typename T>
    struct mem_func_traits;

    template<typename R, typename C, typename... Args>
    struct mem_func_traits<R(C::*)(Args...)>
    {
        using class_type = C;
        using signature = R(Args...);
    };

    template<typename R, typename C, typename... Args>
    struct mem_func_traits<R(C::*)(Args...) const>
    {
        using class_type = C;
        using signature = R(Args...);
    };

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

        DISABLE_COPY(single_delegate_t)

        // Move constructor
        single_delegate_t(single_delegate_t&& other) noexcept
            : _object_ptr(other._object_ptr)
            , _stub_ptr(other._stub_ptr)
            , deleter(other.deleter)
        {
            other._object_ptr = nullptr;
            other._stub_ptr   = nullptr;
            other.deleter     = nullptr;
        }

        // Move assignment
        single_delegate_t& operator=(single_delegate_t&& other) noexcept {
            if (this != &other) {
                // Clean up existing resource
                if (deleter && _object_ptr) {
                    deleter(_object_ptr);
                }

                _object_ptr = other._object_ptr;
                _stub_ptr   = other._stub_ptr;
                deleter     = other.deleter;

                other._object_ptr = nullptr;
                other._stub_ptr   = nullptr;
                other.deleter     = nullptr;
            }
            return *this;
        }

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
            auto* stored = new Functor(std::forward<Functor>(functor));
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

        multicast_delegate_t& operator+=(delegate_type d)
        {
            add(std::move(d));
            return *this;
        }

        multicast_delegate_t& operator-=(const delegate_type& d)
        {
            remove(d);
            return *this;
        }

    private:
        std::vector<delegate_type> _delegates;
    };

    // Helper to bind a member function pointer (deduces signature)
    template<auto member_ptr>
    auto bind_member(typename mem_func_traits<decltype(member_ptr)>::class_type* obj)
    {
        using sig = typename mem_func_traits<decltype(member_ptr)>::signature;
        //static_assert(std::is_same_v<void, std::remove_pointer_t<void(*)() /*dummy*/>>, ""); // noop placeholder
        // Use your existing single_delegate_t bind - requires the signature return type to be void.
        return single_delegate_t<sig>::template bind<member_ptr>(obj);
    }

    // Helper to bind free/static function pointer (deduces signature)
    template<auto func_ptr>
    auto bind_function()
    {
        using sig = decltype(func_ptr); // yields function pointer type
        // Convert function pointer type to function type: e.g. void(*)(A...) -> void(A...)
        using func_t = std::remove_pointer_t<sig>;
        return single_delegate_t<func_t>::template bind<func_ptr>();
    }

    template<typename Callable>
    auto bind_callable(Callable&& callable)
    {
        // Use a small lambda to probe the signature
        // This forces deduction of the Args... from operator() or free-function style
        return [&]<typename R, typename... Args>(R (Callable::*)(Args...) const) -> auto {
            static_assert(std::is_void_v<R>, "Only void-returning callables supported");

            using delegate_t = single_delegate_t<void(Args...)>;

            // Forward to the existing bind<Functor> overload
            return delegate_t::bind(std::forward<Callable>(callable));
        }(&Callable::operator()); // <-- key: pass the member pointer to operator()
    }
} // carrot::utils

#define DECLARE_MULTICAST_DELEGATE(delegate_type, ...) \
using delegate_type = carrot::utils::multicast_delegate_t<void(__VA_ARGS__)>

#define BIND_MEMBER(object_ptr, member_func) \
carrot::utils::bind_member<&std::remove_pointer_t<decltype(object_ptr)>::member_func>(object_ptr)

#define BIND_THIS(member_func) \
carrot::utils::bind_member<&std::decay_t<decltype(*this)>::member_func>(this)

#define BIND_FREE(Func) \
carrot::utils::bind_function<&Func>()

#define BIND_LAMBDA(Lambda) \
carrot::utils::bind_callable(Lambda)
