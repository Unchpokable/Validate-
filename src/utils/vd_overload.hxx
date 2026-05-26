#pragma once

#ifndef VD_OVERLOAD_HXX
#define VD_OVERLOAD_HXX

namespace vd
{
template<typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
} // namespace vd

#endif // VD_OVERLOAD_HXX
