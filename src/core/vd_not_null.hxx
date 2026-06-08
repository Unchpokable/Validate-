#pragma once

#ifndef VD_NOT_NULL_HXX
#define VD_NOT_NULL_HXX

#include <concepts>
#include <cstdio>
#include <exception>
#include <type_traits>

namespace vd::detail
{
[[noreturn]] inline void terminate_on_nullptr()
{
    std::fputs("not_null cannot be constructed with nullptr\n", stderr);
    std::terminate();
}
} // namespace vd::detail

namespace vd
{
template<typename T>
requires std::is_pointer_v<T>
struct not_null final {
    using value_type = std::remove_cv_t<std::remove_pointer_t<T>>;
    using pointer_type = T;
    using const_pointer_type = const value_type*;
    using object_type = std::remove_pointer_t<T>;
    using reference_type = object_type&;
    using const_reference_type = const value_type&;

    constexpr not_null(std::nullptr_t) = delete;

    constexpr not_null(pointer_type ptr) : m_ptr(ptr)
    {
        if(std::is_constant_evaluated()) {
            if(ptr == nullptr) {
                throw "not_null cannot be constructed with nullptr";
            }
        }
        else {
            if(ptr == nullptr) [[unlikely]] {
                vd::detail::terminate_on_nullptr();
            }
        }
    }

    template<typename SmartPtr>
    requires requires(const SmartPtr& ptr) {
        { ptr.get() } -> std::convertible_to<pointer_type>;
    }
    constexpr not_null(const SmartPtr& ptr) : m_ptr(ptr.get())
    {
        if(std::is_constant_evaluated()) {
            if(m_ptr == nullptr) {
                throw "not_null cannot be constructed with nullptr";
            }
        }
        else {
            if(m_ptr == nullptr) [[unlikely]] {
                vd::detail::terminate_on_nullptr();
            }
        }
    }

    template<typename U>
    requires std::is_convertible_v<U, T>
    constexpr not_null(const not_null<U>& other) noexcept : m_ptr(other.get())
    {
    }

    constexpr not_null(const not_null&) = default;
    constexpr not_null(not_null&&) = default;

    constexpr not_null& operator=(const not_null&) = default;
    constexpr not_null& operator=(not_null&&) = default;

    constexpr not_null& operator=(std::nullptr_t) = delete;

    constexpr not_null& operator=(pointer_type ptr)
    {
        if(std::is_constant_evaluated()) {
            if(ptr == nullptr) {
                throw "not_null cannot be constructed with nullptr";
            }
        }
        else {
            if(ptr == nullptr) [[unlikely]] {
                vd::detail::terminate_on_nullptr();
            }
        }

        m_ptr = ptr;
        return *this;
    }

    constexpr pointer_type get() const noexcept
    {
        return m_ptr;
    }

    constexpr reference_type operator*() const noexcept
    {
        return *m_ptr;
    }

    constexpr pointer_type operator->() const noexcept
    {
        return m_ptr;
    }

    constexpr operator pointer_type() const noexcept
    {
        return m_ptr;
    }

    friend constexpr auto operator<=>(not_null, not_null) = default;
    friend constexpr bool operator==(not_null, not_null) = default;

private:
    pointer_type m_ptr;
};
} // namespace vd

#endif // VD_NOT_NULL_HXX
