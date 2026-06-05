#pragma once

#ifndef VD_MEMORY_HXX
#define VD_MEMORY_HXX

#include "core/vd_always_false.hxx"
#include "core/vd_result.hxx"

namespace vd::memory::detail
{
template<typename T>
concept pointer_like = requires(const T& ptr) {
    { ptr == nullptr } -> std::convertible_to<bool>;
};

struct not_null_t final {
    template<typename T>
    vd::result operator()(const T& ptr) const
    {
        if constexpr(pointer_like<T>) {
            if(ptr == nullptr) {
                return vd::result::failed({ "Pointer must not be null" });
            }
            return vd::result::ok();
        }
        else {
            static_assert(vd::always_false_v<T>,
                "vd::memory::not_null can only be used with pointer-like types "
                "(raw pointers, std::shared_ptr, std::unique_ptr, QPointer, etc.)");
        }
    }
};

} // namespace vd::memory::detail

namespace vd::memory
{
inline constexpr detail::not_null_t not_null;
} // namespace vd::memory

#endif
