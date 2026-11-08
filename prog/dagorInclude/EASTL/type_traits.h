#pragma once
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <type_traits>

namespace eastl {
  template <typename T> using is_pointer = std::is_pointer<T>;
  template <typename T> inline constexpr bool is_pointer_v = std::is_pointer_v<T>;
  template <typename T> inline constexpr bool is_integral_v = std::is_integral_v<T>;
  template <typename T> inline constexpr bool is_floating_point_v = std::is_floating_point_v<T>;
  template <typename T> inline constexpr bool is_arithmetic_v = std::is_arithmetic_v<T>;
  template <typename T> inline constexpr bool is_signed_v = std::is_signed_v<T>;
  template <typename T> inline constexpr bool is_unsigned_v = std::is_unsigned_v<T>;
  template <typename T> inline constexpr bool is_trivially_copyable_v = std::is_trivially_copyable_v<T>;
  template <typename T> inline constexpr bool is_enum_v = std::is_enum_v<T>;
  template <typename T, typename U> inline constexpr bool is_same_v = std::is_same_v<T, U>;
  template <bool B, typename T = void> using enable_if_t = std::enable_if_t<B, T>;
  template <typename T> using underlying_type_t = std::underlying_type_t<T>;
  template <typename T> using make_unsigned_t = std::make_unsigned_t<T>;
  template <typename T> using make_signed_t = std::make_signed_t<T>;
  template <typename T> using remove_cv_t = std::remove_cv_t<T>;
  template <typename T> using remove_reference_t = std::remove_reference_t<T>;
}
