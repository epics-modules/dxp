/*
 * Copyright (c) 2004, X-ray Instrumentation Associates
 *               2005-2010, XIA LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 *   * Redistributions in binary form must reproduce the 
 *     above copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 *   * Neither the name of X-ray Instrumentation Associates 
 *     nor the names of its contributors may be used to endorse 
 *     or promote products derived from this software without 
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *  
 *
 * $Id: xia_common.h 15965 2010-06-10 20:59:12Z patrick $
 *
 */


#ifndef __XIA_COMMON_H__
#define __XIA_COMMON_H__

#include <string.h>
#include <math.h>

/* Define the length of the error reporting string info_string */
#define INFO_LEN 400
/* Define the length of the line string used to read in files */
#define XIA_LINE_LEN 132

/** Typedefs **/
typedef unsigned char  byte_t;
typedef unsigned char  boolean_t;
typedef unsigned short parameter_t;
typedef unsigned short flag_t;

#ifdef LINUX
typedef int             HANDLE;
typedef unsigned char   UCHAR;
typedef unsigned short  USHORT;
typedef unsigned short* PUSHORT;
typedef unsigned int    ULONG;
typedef unsigned long*  PULONG;
#endif /* LINUX */

/** MACROS **/
#define TRUE_  (1 == 1)
#define FALSE_ (1 == 0)

#define UNUSED(x)   ((x) = (x))
#define STREQ(x, y) (strcmp((x), (y)) == 0)
#define STRNEQ(x, y) (strncmp((x), (y), strlen(y)) == 0)
#define ROUND(x)    ((x) < 0.0 ? ceil((x) - 0.5) : floor((x) + 0.5))
#define PRINT_NON_NULL(x) ((x) == NULL ? "NULL" : (x))
#define BYTE_TO_WORD(lo, hi) (unsigned short)(((unsigned short)(hi) << 8) | (lo))
#define WORD_TO_LONG(lo, hi) (unsigned long)(((unsigned long)(hi) << 16) | (lo)) 
#define LO_BYTE(word) (byte_t)((word) & 0xFF)
#define HI_BYTE(word) (byte_t)(((word) >> 8) & 0xFF)
#define LO_WORD(dword) ((dword) & 0xFFFF)
#define HI_WORD(dword) (((dword) >> 16) & 0xFFFF)
#define MAKE_LOWER_CASE(s, i) for ((i) = 0; (i) < strlen((s)); (i)++) (s)[i] = (char)tolower((int)((s)[i]))
#define N_ELEMS(x) (sizeof(x) / sizeof((x)[0]))

/* There is a known issue with glibc on Cygwin where ctype routines
 * that are passed an input > 0x7F return garbage.
 */
#ifdef CYGWIN32
#define CTYPE_CHAR(c) ((unsigned char)(c))
#else
#define CTYPE_CHAR(c) (c)
#endif /* CYGWIN32 */


/* These macros already exist on Linux, so they need to be protected */

#ifndef MIN
#define MIN(x, y)  ((x) < (y) ? (x) : (y))
#endif /* MIN */

#ifndef MAX
#define MAX(x, y)  ((x) > (y) ? (x) : (y))
#endif /* MAX */

#endif /* __XIA_COMMON_H__ */
