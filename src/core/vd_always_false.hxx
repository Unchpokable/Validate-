#pragma once

#ifndef VD_ALWAYS_FALSE_HXX
#define VD_ALWAYS_FALSE_HXX

#include <type_traits>

namespace vd
{
template<typename T>
struct always_false : std::false_type {};

template<>
struct always_false<void> : std::false_type {};

template<typename T>
inline constexpr bool always_false_v = always_false<T>::value;

inline constexpr bool always_false_vt = always_false<void>::value;
} // namespace vd

#endif // VD_ALWAYS_FALSE_HXX