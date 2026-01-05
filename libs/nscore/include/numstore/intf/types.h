#pragma once

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
 *   Core type definitions for integer (u8-u64, i8-i64) and floating-point types,
 *   complex numbers, domain-specific types (page numbers, LSN, etc.), and limits.
 */

/*//////////////////////////////// CORE TYPES */
/* ILP32, LP64, LLP64, etc. */
// system

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define U8_MIN 0
#define U16_MIN 0
#define U32_MIN 0
#define U64_MIN 0
#define U8_MAX ((u8) ~(u8)0)
#define U16_MAX ((u16) ~(u16)0)
#define U32_MAX ((u32) ~(u32)0)
#define U64_MAX ((u64) ~(u64)0)

// MSVC uses different format specifiers for 64-bit integers
#ifdef _MSC_VER
#define PRIu8 "hhu"
#define PRIu16 "hu"
#define PRIu32 "u"
#define PRIu64 "I64u"
#define PRIi8 "hhd"
#define PRIi16 "hd"
#define PRIi32 "d"
#define PRIi64 "I64d"
#else
#define PRIu8 "hhu"
#define PRIu16 "hu"
#define PRIu32 "u"
#define PRIu64 "lu"
#define PRIi8 "hhd"
#define PRIi16 "hd"
#define PRIi32 "d"
#define PRIi64 "ld"
#endif

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

/* ABS_MAX is the max of abs(value) */
#define I8_MAX ((i8) (U8_MAX >> 1))
#define I8_ABS_MAX ((u8) (I8_MAX) + 1)
#define I16_MAX ((i16) (U16_MAX >> 1))
#define I16_ABS_MAX ((u16) (I16_MAX) + 1)
#define I32_MAX ((i32) (U32_MAX >> 1))
#define I32_ABS_MAX ((u32) (I32_MAX) + 1)
#define I64_MAX ((i64) (U64_MAX >> 1))
#define I64_ABS_MAX ((u64) (I64_MAX) + 1)
#define I8_MIN ((i8) (~I8_MAX))
#define I16_MIN ((i16) (~I16_MAX))
#define I32_MIN ((i32) (~I32_MAX))
#define I64_MIN ((i64) (~I64_MAX))

#define F16_MAX 65504.0f
#define F16_MIN (-65504.0f)
#define F32_MAX 3.4028235e+38f
#define F32_MIN (-3.4028235e+38f)
#define F64_MAX 1.7976931348623157e+308
#define F64_MIN (-1.7976931348623157e+308)
#define F128_MAX F64_MAX // TODO
#define F128_MIN F64_MIN // TODO
#define F256_MAX 1.6113e+78913L
#define F256_MIN (-1.6113e+78913L)

#define PRId8 "hhd"
#define PRId16 "hd"
#define PRId32 "d"
#define PRId64 "ld"

typedef u16 f16;
typedef float f32;
typedef double f64;
typedef long double f128;

typedef f16 cf32[2];
#ifdef _MSC_VER
// MSVC doesn't support C11 _Complex, use array representation instead
typedef float cf64[2];
typedef double cf128[2];
typedef long double cf256[2];
#define I 0 // MSVC doesn't support complex imaginary unit
#else
typedef float _Complex cf64;
typedef double _Complex cf128;
typedef long double _Complex cf256;
#ifndef I
#define I (__extension__ 1.0iF)
#endif
#endif

typedef i8 ci16[2];
typedef i16 ci32[2];
typedef i32 ci64[2];
typedef i64 ci128[2];

typedef u8 cu16[2];
typedef u16 cu32[2];
typedef u32 cu64[2];
typedef u64 cu128[2];

typedef int bool;
#define true 1
#define false 0

/*//////////////////////////////// DOMAIN TYPES */
typedef u32 t_size;  /* Represents the size of a single type in bytes */
typedef i32 st_size; /* Represents the size of a single type in bytes */
typedef u32 p_size;  /* To index inside a page */
typedef i32 sp_size; /* Signed p size */
typedef u64 b_size;  /* Bytes size to index into a contiguous rope bytes */
typedef i64 sb_size; /* Signed b size */
typedef u64 pgno;    /* Page number */
typedef i64 spgno;   /* Signed Page number */
typedef u64 txid;    /* Transaction id */
typedef i64 stxid;   /* Signed Transaction id */
typedef i64 slsn;    /* Wall Index (often called LSN) */
typedef u64 lsn;     /* Wall Index (often called LSN) */
typedef u8 pgh;      /* Page header */
typedef u8 wlh;      /* Wal Header */

#define PGNO_NULL U64_MAX

#define PRt_size PRIu32
#define PRsp_size PRId32
#define PRp_size PRIu32
#define PRb_size PRIu64
#define PRsb_size PRId64
#define PRspgno PRId64
#define PRpgno PRIu64
#define PRpgh PRIu8
#define PRtxid PRIu64
#define PRstxid PRId64
#define PRlsn PRIu64
#define PRslsn PRId64

#ifndef NULL
#define NULL (void *)0
#endif
