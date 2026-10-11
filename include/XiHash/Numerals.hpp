#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace XiHash::Numerals {

template<class T, std::size_t N>
using Vector = std::array<T, N>;

template<class T> constexpr Vector<T, 2> make2(T x, T y) { return {x, y}; }
template<class T> constexpr Vector<T, 3> make3(T x, T y, T z) { return {x, y, z}; }
template<class T> constexpr Vector<T, 4> make4(T x, T y, T z, T w) { return {x, y, z, w}; }

template<class T, std::size_t N>
constexpr Vector<T, N> add(const Vector<T, N>& a, const Vector<T, N>& b) {
    Vector<T, N> result{};
    for (std::size_t i = 0; i < N; ++i) result[i] = a[i] + b[i];
    return result;
}

template<class T, std::size_t N>
constexpr Vector<T, N> subtract(const Vector<T, N>& a, const Vector<T, N>& b) {
    Vector<T, N> result{};
    for (std::size_t i = 0; i < N; ++i) result[i] = a[i] - b[i];
    return result;
}

template<class T, std::size_t N>
constexpr Vector<T, N> scale(const Vector<T, N>& value, T scalar) {
    Vector<T, N> result{};
    for (std::size_t i = 0; i < N; ++i) result[i] = value[i] * scalar;
    return result;
}

template<class T, std::size_t N>
constexpr T dot(const Vector<T, N>& a, const Vector<T, N>& b) {
    T result{};
    for (std::size_t i = 0; i < N; ++i) result += a[i] * b[i];
    return result;
}

template<class T, std::size_t N>
inline T length(const Vector<T, N>& value) {
    return static_cast<T>(std::sqrt(static_cast<double>(dot(value, value))));
}

template<class T, std::size_t N>
inline Vector<T, N> normalize(const Vector<T, N>& value) {
    const T magnitude = length(value);
    if (magnitude == T{}) return {};
    Vector<T, N> result{};
    for (std::size_t i = 0; i < N; ++i) result[i] = value[i] / magnitude;
    return result;
}

template<class T>
constexpr Vector<T, 3> cross(const Vector<T, 3>& a, const Vector<T, 3>& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}

#define XIHASH_NUMERALS_OVERLOADS(SUFFIX, TYPE) \
    inline Vector<TYPE, 2> Vector2##SUFFIX(TYPE x, TYPE y) { return make2(x, y); } \
    inline Vector<TYPE, 3> Vector3##SUFFIX(TYPE x, TYPE y, TYPE z) { return make3(x, y, z); } \
    inline Vector<TYPE, 4> Vector4##SUFFIX(TYPE x, TYPE y, TYPE z, TYPE w) { return make4(x, y, z, w); } \
    inline Vector<TYPE, 2> add2##SUFFIX(Vector<TYPE, 2> a, Vector<TYPE, 2> b) { return add(a, b); } \
    inline Vector<TYPE, 3> add3##SUFFIX(Vector<TYPE, 3> a, Vector<TYPE, 3> b) { return add(a, b); } \
    inline Vector<TYPE, 4> add4##SUFFIX(Vector<TYPE, 4> a, Vector<TYPE, 4> b) { return add(a, b); } \
    inline Vector<TYPE, 2> subtract2##SUFFIX(Vector<TYPE, 2> a, Vector<TYPE, 2> b) { return subtract(a, b); } \
    inline Vector<TYPE, 3> subtract3##SUFFIX(Vector<TYPE, 3> a, Vector<TYPE, 3> b) { return subtract(a, b); } \
    inline Vector<TYPE, 4> subtract4##SUFFIX(Vector<TYPE, 4> a, Vector<TYPE, 4> b) { return subtract(a, b); } \
    inline Vector<TYPE, 2> scale2##SUFFIX(Vector<TYPE, 2> a, TYPE s) { return scale(a, s); } \
    inline Vector<TYPE, 3> scale3##SUFFIX(Vector<TYPE, 3> a, TYPE s) { return scale(a, s); } \
    inline Vector<TYPE, 4> scale4##SUFFIX(Vector<TYPE, 4> a, TYPE s) { return scale(a, s); } \
    inline TYPE dot2##SUFFIX(Vector<TYPE, 2> a, Vector<TYPE, 2> b) { return dot(a, b); } \
    inline TYPE dot3##SUFFIX(Vector<TYPE, 3> a, Vector<TYPE, 3> b) { return dot(a, b); } \
    inline TYPE dot4##SUFFIX(Vector<TYPE, 4> a, Vector<TYPE, 4> b) { return dot(a, b); } \
    inline TYPE length2##SUFFIX(Vector<TYPE, 2> a) { return length(a); } \
    inline TYPE length3##SUFFIX(Vector<TYPE, 3> a) { return length(a); } \
    inline TYPE length4##SUFFIX(Vector<TYPE, 4> a) { return length(a); } \
    inline Vector<TYPE, 2> normalize2##SUFFIX(Vector<TYPE, 2> a) { return normalize(a); } \
    inline Vector<TYPE, 3> normalize3##SUFFIX(Vector<TYPE, 3> a) { return normalize(a); } \
    inline Vector<TYPE, 4> normalize4##SUFFIX(Vector<TYPE, 4> a) { return normalize(a); } \
    inline Vector<TYPE, 3> cross3##SUFFIX(Vector<TYPE, 3> a, Vector<TYPE, 3> b) { return cross(a, b); }

inline Vector<float, 2> Vector2(float x, float y) { return make2(x, y); }
inline Vector<float, 3> Vector3(float x, float y, float z) { return make3(x, y, z); }
inline Vector<float, 4> Vector4(float x, float y, float z, float w) { return make4(x, y, z, w); }
inline Vector<float, 2> add2(Vector<float, 2> a, Vector<float, 2> b) { return add(a, b); }
inline Vector<float, 3> add3(Vector<float, 3> a, Vector<float, 3> b) { return add(a, b); }
inline Vector<float, 4> add4(Vector<float, 4> a, Vector<float, 4> b) { return add(a, b); }
inline Vector<float, 2> subtract2(Vector<float, 2> a, Vector<float, 2> b) { return subtract(a, b); }
inline Vector<float, 3> subtract3(Vector<float, 3> a, Vector<float, 3> b) { return subtract(a, b); }
inline Vector<float, 4> subtract4(Vector<float, 4> a, Vector<float, 4> b) { return subtract(a, b); }
inline Vector<float, 2> scale2(Vector<float, 2> a, float s) { return scale(a, s); }
inline Vector<float, 3> scale3(Vector<float, 3> a, float s) { return scale(a, s); }
inline Vector<float, 4> scale4(Vector<float, 4> a, float s) { return scale(a, s); }
inline float dot2(Vector<float, 2> a, Vector<float, 2> b) { return dot(a, b); }
inline float dot3(Vector<float, 3> a, Vector<float, 3> b) { return dot(a, b); }
inline float dot4(Vector<float, 4> a, Vector<float, 4> b) { return dot(a, b); }
inline float length2(Vector<float, 2> a) { return length(a); }
inline float length3(Vector<float, 3> a) { return length(a); }
inline float length4(Vector<float, 4> a) { return length(a); }
inline Vector<float, 2> normalize2(Vector<float, 2> a) { return normalize(a); }
inline Vector<float, 3> normalize3(Vector<float, 3> a) { return normalize(a); }
inline Vector<float, 4> normalize4(Vector<float, 4> a) { return normalize(a); }
inline Vector<float, 3> cross3(Vector<float, 3> a, Vector<float, 3> b) { return cross(a, b); }

XIHASH_NUMERALS_OVERLOADS(i, std::int32_t)
XIHASH_NUMERALS_OVERLOADS(u, std::uint32_t)
XIHASH_NUMERALS_OVERLOADS(d, double)
#undef XIHASH_NUMERALS_OVERLOADS

} // namespace XiHash::Numerals
