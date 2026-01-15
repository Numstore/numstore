/*
 * Copyright 2025 Theo Lincke
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 *   Overflow-safe arithmetic macros for fixed-width numeric types (u8, u16, u32,
 *   u64, i8, i16, i32, i64, f32, f64). Provides checked add, subtract, multiply,
 *   and divide operations that detect overflow and division by zero.
 */

// core
#include <numstore/core/error.h>
#include <numstore/core/signatures.h>

#include <math.h>
#include <stdint.h>

// system
/*
 * Overflow-checking macros for fixed-width numeric types.
 * On overflow or divide-by-zero, they return false.
 *
 * Assumes operands and destination are already of correct type.
 *
 * Usage:
 *   u8 x; if (SAFE_ADD_U8(&x, a, b)) { ... }
 */

/*///////////////////////////////////// UNSIGNED INT */

#ifdef _MSC_VER
// MSVC doesn't have __builtin_*_overflow, use manual checks
#define SAFE_ADD_U8(dest, a, b) \
  ((u8)(a) <= (UINT8_MAX - (u8)(b)) ? (*(dest) = (u8)(a) + (u8)(b), true) : false)
#define SAFE_SUB_U8(dest, a, b) \
  ((u8)(a) >= (u8)(b) ? (*(dest) = (u8)(a) - (u8)(b), true) : false)
#define SAFE_MUL_U8(dest, a, b) \
  (((u8)(a) == 0 || (u8)(b) <= (UINT8_MAX / (u8)(a))) ? (*(dest) = (u8)(a) * (u8)(b), true) : false)

#define SAFE_ADD_U16(dest, a, b) \
  ((u16)(a) <= (UINT16_MAX - (u16)(b)) ? (*(dest) = (u16)(a) + (u16)(b), true) : false)
#define SAFE_SUB_U16(dest, a, b) \
  ((u16)(a) >= (u16)(b) ? (*(dest) = (u16)(a) - (u16)(b), true) : false)
#define SAFE_MUL_U16(dest, a, b) \
  (((u16)(a) == 0 || (u16)(b) <= (UINT16_MAX / (u16)(a))) ? (*(dest) = (u16)(a) * (u16)(b), true) : false)

#define SAFE_ADD_U32(dest, a, b) \
  ((u32)(a) <= (UINT32_MAX - (u32)(b)) ? (*(dest) = (u32)(a) + (u32)(b), true) : false)
#define SAFE_SUB_U32(dest, a, b) \
  ((u32)(a) >= (u32)(b) ? (*(dest) = (u32)(a) - (u32)(b), true) : false)
#define SAFE_MUL_U32(dest, a, b) \
  (((u32)(a) == 0 || (u32)(b) <= (UINT32_MAX / (u32)(a))) ? (*(dest) = (u32)(a) * (u32)(b), true) : false)

#define SAFE_ADD_U64(dest, a, b) \
  ((u64)(a) <= (UINT64_MAX - (u64)(b)) ? (*(dest) = (u64)(a) + (u64)(b), true) : false)
#else
#define SAFE_ADD_U8(dest, a, b) \
  (!__builtin_add_overflow (a, b, dest))
#define SAFE_SUB_U8(dest, a, b) \
  (!__builtin_sub_overflow (a, b, dest))
#define SAFE_MUL_U8(dest, a, b) \
  (!__builtin_mul_overflow (a, b, dest))

#define SAFE_ADD_U16(dest, a, b) \
  (!__builtin_add_overflow (a, b, dest))
#define SAFE_SUB_U16(dest, a, b) \
  (!__builtin_sub_overflow (a, b, dest))
#define SAFE_MUL_U16(dest, a, b) \
  (!__builtin_mul_overflow (a, b, dest))

#define SAFE_ADD_U32(dest, a, b) \
  (!__builtin_add_overflow (a, b, dest))
#define SAFE_SUB_U32(dest, a, b) \
  (!__builtin_sub_overflow (a, b, dest))
#define SAFE_MUL_U32(dest, a, b) \
  (!__builtin_mul_overflow (a, b, dest))

#define SAFE_ADD_U64(dest, a, b) \
  (!__builtin_add_overflow (a, b, dest))
#endif

#define SAFE_DIV_U8(dest, a, b) \
  ((b) != 0 ? (*(dest) = (a) / (b), true) : false)

#define SAFE_DIV_U16(dest, a, b) \
  ((b) != 0 ? (*(dest) = (a) / (b), true) : false)

#define SAFE_DIV_U32(dest, a, b) \
  ((b) != 0 ? (*(dest) = (a) / (b), true) : false)

HEADER_FUNC err_t
safe_add_u64 (u64 *dest, u64 a, u64 b, error *e)
{
  if (!SAFE_ADD_U64 (dest, a, b))
    {
      return error_causef (e, ERR_ARITH, "Overflow");
    }
  return SUCCESS;
}

#ifdef _MSC_VER
#define SAFE_SUB_U64(dest, a, b) \
  ((u64)(a) >= (u64)(b) ? (*(dest) = (u64)(a) - (u64)(b), true) : false)
#define SAFE_MUL_U64(dest, a, b) \
  (((u64)(a) == 0 || (u64)(b) <= (UINT64_MAX / (u64)(a))) ? (*(dest) = (u64)(a) * (u64)(b), true) : false)
#else
#define SAFE_SUB_U64(dest, a, b) \
  (!__builtin_sub_overflow (a, b, dest))
#define SAFE_MUL_U64(dest, a, b) \
  (!__builtin_mul_overflow (a, b, dest))
#endif

#define SAFE_DIV_U64(dest, a, b) \
  ((b) != 0 ? (*(dest) = (a) / (b), true) : false)

/*///////////////////////////////////// SIGNED INT */

#ifdef _MSC_VER
// MSVC signed integer overflow checks
#define SAFE_ADD_I8(dest, a, b) \
  ((((b) > 0 && (i8)(a) <= (INT8_MAX - (i8)(b))) || ((b) <= 0 && (i8)(a) >= (INT8_MIN - (i8)(b)))) ? (*(dest) = (i8)(a) + (i8)(b), true) : false)
#define SAFE_SUB_I8(dest, a, b) \
  ((((b) < 0 && (i8)(a) <= (INT8_MAX + (i8)(b))) || ((b) >= 0 && (i8)(a) >= (INT8_MIN + (i8)(b)))) ? (*(dest) = (i8)(a) - (i8)(b), true) : false)
#define SAFE_MUL_I8(dest, a, b) \
  ((((a) == 0 || (b) == 0) || (((a) > 0 && (b) > 0 && (i8)(a) <= (INT8_MAX / (i8)(b))) || ((a) < 0 && (b) < 0 && (i8)(a) >= (INT8_MAX / (i8)(b))) || ((a) > 0 && (b) < 0 && (i8)(b) >= (INT8_MIN / (i8)(a))) || ((a) < 0 && (b) > 0 && (i8)(a) >= (INT8_MIN / (i8)(b))))) ? (*(dest) = (i8)(a) * (i8)(b), true) : false)
#else
#define SAFE_ADD_I8(dest, a, b) \
  (!__builtin_add_overflow (a, b, dest))
#define SAFE_SUB_I8(dest, a, b) \
  (!__builtin_sub_overflow (a, b, dest))
#define SAFE_MUL_I8(dest, a, b) \
  (!__builtin_mul_overflow (a, b, dest))
#endif

#define SAFE_DIV_I8(dest, a, b) \
  ((b) != 0 && !((a) == I8_MIN && (b) == -1) ? (*(dest) = (a) / (b), true) : false)

#ifdef _MSC_VER
#define SAFE_ADD_I16(dest, a, b) \
  ((((b) > 0 && (i16)(a) <= (INT16_MAX - (i16)(b))) || ((b) <= 0 && (i16)(a) >= (INT16_MIN - (i16)(b)))) ? (*(dest) = (i16)(a) + (i16)(b), true) : false)
#define SAFE_SUB_I16(dest, a, b) \
  ((((b) < 0 && (i16)(a) <= (INT16_MAX + (i16)(b))) || ((b) >= 0 && (i16)(a) >= (INT16_MIN + (i16)(b)))) ? (*(dest) = (i16)(a) - (i16)(b), true) : false)
#define SAFE_MUL_I16(dest, a, b) \
  ((((a) == 0 || (b) == 0) || (((a) > 0 && (b) > 0 && (i16)(a) <= (INT16_MAX / (i16)(b))) || ((a) < 0 && (b) < 0 && (i16)(a) >= (INT16_MAX / (i16)(b))) || ((a) > 0 && (b) < 0 && (i16)(b) >= (INT16_MIN / (i16)(a))) || ((a) < 0 && (b) > 0 && (i16)(a) >= (INT16_MIN / (i16)(b))))) ? (*(dest) = (i16)(a) * (i16)(b), true) : false)

#define SAFE_ADD_I32(dest, a, b) \
  ((((b) > 0 && (i32)(a) <= (INT32_MAX - (i32)(b))) || ((b) <= 0 && (i32)(a) >= (INT32_MIN - (i32)(b)))) ? (*(dest) = (i32)(a) + (i32)(b), true) : false)
#define SAFE_SUB_I32(dest, a, b) \
  ((((b) < 0 && (i32)(a) <= (INT32_MAX + (i32)(b))) || ((b) >= 0 && (i32)(a) >= (INT32_MIN + (i32)(b)))) ? (*(dest) = (i32)(a) - (i32)(b), true) : false)
#define SAFE_MUL_I32(dest, a, b) \
  ((((a) == 0 || (b) == 0) || (((a) > 0 && (b) > 0 && (i32)(a) <= (INT32_MAX / (i32)(b))) || ((a) < 0 && (b) < 0 && (i32)(a) >= (INT32_MAX / (i32)(b))) || ((a) > 0 && (b) < 0 && (i32)(b) >= (INT32_MIN / (i32)(a))) || ((a) < 0 && (b) > 0 && (i32)(a) >= (INT32_MIN / (i32)(b))))) ? (*(dest) = (i32)(a) * (i32)(b), true) : false)

#define SAFE_ADD_I64(dest, a, b) \
  ((((b) > 0 && (i64)(a) <= (INT64_MAX - (i64)(b))) || ((b) <= 0 && (i64)(a) >= (INT64_MIN - (i64)(b)))) ? (*(dest) = (i64)(a) + (i64)(b), true) : false)
#define SAFE_SUB_I64(dest, a, b) \
  ((((b) < 0 && (i64)(a) <= (INT64_MAX + (i64)(b))) || ((b) >= 0 && (i64)(a) >= (INT64_MIN + (i64)(b)))) ? (*(dest) = (i64)(a) - (i64)(b), true) : false)
#define SAFE_MUL_I64(dest, a, b) \
  ((((a) == 0 || (b) == 0) || (((a) > 0 && (b) > 0 && (i64)(a) <= (INT64_MAX / (i64)(b))) || ((a) < 0 && (b) < 0 && (i64)(a) >= (INT64_MAX / (i64)(b))) || ((a) > 0 && (b) < 0 && (i64)(b) >= (INT64_MIN / (i64)(a))) || ((a) < 0 && (b) > 0 && (i64)(a) >= (INT64_MIN / (i64)(b))))) ? (*(dest) = (i64)(a) * (i64)(b), true) : false)
#else
#define SAFE_ADD_I16(dest, a, b) \
  (!__builtin_add_overflow (a, b, dest))
#define SAFE_SUB_I16(dest, a, b) \
  (!__builtin_sub_overflow (a, b, dest))
#define SAFE_MUL_I16(dest, a, b) \
  (!__builtin_mul_overflow (a, b, dest))

#define SAFE_ADD_I32(dest, a, b) \
  (!__builtin_add_overflow (a, b, dest))
#define SAFE_SUB_I32(dest, a, b) \
  (!__builtin_sub_overflow (a, b, dest))
#define SAFE_MUL_I32(dest, a, b) \
  (!__builtin_mul_overflow (a, b, dest))

#define SAFE_ADD_I64(dest, a, b) \
  (!__builtin_add_overflow (a, b, dest))
#define SAFE_SUB_I64(dest, a, b) \
  (!__builtin_sub_overflow (a, b, dest))
#define SAFE_MUL_I64(dest, a, b) \
  (!__builtin_mul_overflow (a, b, dest))
#endif

#define SAFE_DIV_I16(dest, a, b) \
  ((b) != 0 && !((a) == I16_MIN && (b) == -1) ? (*(dest) = (a) / (b), true) : false)

#define SAFE_DIV_I32(dest, a, b) \
  ((b) != 0 && !((a) == I32_MIN && (b) == -1) ? (*(dest) = (a) / (b), true) : false)

#define SAFE_DIV_I64(dest, a, b) \
  ((b) != 0 && !((a) == I64_MIN && (b) == -1) ? (*(dest) = (a) / (b), true) : false)

/*///////////////////////////////////// FLOAT */

#define SAFE_ADD_F32(dest, a, b) \
  ((*(dest) = (a) + (b)), isfinite (*(dest)))

#define SAFE_SUB_F32(dest, a, b) \
  ((*(dest) = (a) - (b)), isfinite (*(dest)))

#define SAFE_MUL_F32(dest, a, b) \
  ((*(dest) = (a) * (b)), isfinite (*(dest)))

#define SAFE_DIV_F32(dest, a, b) \
  ((b) != 0.0f ? (*(dest) = (a) / (b), isfinite (*(dest))) : false)

#define SAFE_ADD_F64(dest, a, b) \
  ((*(dest) = (a) + (b)), isfinite (*(dest)))

#define SAFE_SUB_F64(dest, a, b) \
  ((*(dest) = (a) - (b)), isfinite (*(dest)))

#define SAFE_MUL_F64(dest, a, b) \
  ((*(dest) = (a) * (b)), isfinite (*(dest)))

#define SAFE_DIV_F64(dest, a, b) \
  ((b) != 0.0 ? (*(dest) = (a) / (b), isfinite (*(dest))) : false)
