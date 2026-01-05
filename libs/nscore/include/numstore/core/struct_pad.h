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
 *   Commented-out experimental utilities for struct padding and memory layout
 *   conversions between packed and unpacked representations (not currently active).
 */

/**
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct stprops {
        struct {
                t_size ofst; 									// What byte does this field start
                bool iscmplx;
                union {
                        struct stprops cmplxprops; 	// Embedded complex type properties
                        t_size size;								// This element is a primitive type and is layed out in memory contiguously - this is the size
                };
        } * fields; 										// A list of fields
        u32 flen; 											// Length of fields
};

struct stprops type_to_stprops(struct type t);
*/

/**
 * pcklen - The length (in bytes) of pck_dest
 * stidx 	- For half writes (e.g. struct { int a } we previously only wrote 2 bytes
 * 					into
 * 			  - note that [st_src] could have written partial fields to [pck_dest]
 * 			    and pckidx could reflect that. For example struct { int a }; could have
 * 			    written only 2 bytes from a. pckidx now = 2, so it is the job of this function
 * 			    to do memcpy(pck_dest, &((u8*)a)[2]). pck_dest is pointing to zero
 */
/*
void i_memcpy_sttopck(
                void *pck_dest,
                void *st_src,
                struct stprops props,
                u32 pcklen,
                u32 pckidx);

void i_memcpy_pcktost(void *st_dest, void *pck_src, struct stprops props, u32 nelems);
*/
