#pragma once

#include <cstdint>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

namespace Nerine
{

using f32 = float;
using f64 = double;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using mat3 = glm::mat3;
using mat4 = glm::mat4;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using ivec2 = glm::ivec2;

#define NON_COPYABLE(cls)                                                                          \
    cls(const cls&) = delete;                                                                      \
    cls& operator=(const cls&) = delete

#define NON_MOVEABLE(cls)                                                                          \
    cls(cls&&) noexcept = delete;                                                                  \
    cls& operator=(cls&&) noexcept = delete

// XXX: use underlying_type instead of hardcoding u32
#define ENUM_CLASS_FLAG_OPERATORS(T)                                                               \
    inline T operator|(T a, T b)                                                                   \
    {                                                                                              \
        return T(u32(a) | u32(b));                                                                 \
    }                                                                                              \
    inline T operator&(T a, T b)                                                                   \
    {                                                                                              \
        return T(u32(a) & u32(b));                                                                 \
    }                                                                                              \
    inline T operator~(T a)                                                                        \
    {                                                                                              \
        return T(~u32(a));                                                                         \
    }                                                                                              \
    inline bool operator!(T a)                                                                     \
    {                                                                                              \
        return u32(a) == 0;                                                                        \
    }                                                                                              \
    inline bool operator==(T a, u32 b)                                                             \
    {                                                                                              \
        return u32(a) == b;                                                                        \
    }                                                                                              \
    inline bool operator!=(T a, u32 b)                                                             \
    {                                                                                              \
        return u32(a) != b;                                                                        \
    }

// XXX: Provide compiler for other compilers.
#define PACKED_STRUCT __attribute__((packed, aligned(1)))

struct PACKED_STRUCT GPUVec4
{
    float x, y, z, w;

    GPUVec4() = default;
    explicit GPUVec4(float v) : x(v), y(v), z(v), w(v)
    {
    }
    GPUVec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d)
    {
    }
    explicit GPUVec4(const vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w)
    {
    }
};

struct PACKED_STRUCT GPUMat4
{
    float data[16];

    GPUMat4() = default;
    explicit GPUMat4(const glm::mat4& m)
    {
        memcpy(data, glm::value_ptr(m), 16 * sizeof(float));
    }
};

} // namespace Nerine