/* ISO C9x  7.18  Integer types <stdint.h>
 * Based on ISO/IEC SC22/WG14 9899 Committee draft (SC22 N2794)
 *
 *  THIS SOFTWARE IS NOT COPYRIGHTED
 *
 *  Contributor: Danny Smith <danny_r_smith_2001@yahoo.co.nz>
 *
 *  This source code is offered for use in the public domain. You may
 *  use, modify or distribute it freely.
 *
 *  This code is distributed in the hope that it will be useful but
 *  WITHOUT ANY WARRANTY. ALL WARRANTIES, EXPRESS OR IMPLIED ARE HEREBY
 *  DISCLAIMED. This includes but is not limited to warranties of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  Date: 2000-12-02
 */


#ifndef _STDINT_H
#define _STDINT_H
#define __need_wint_t
#define __need_wchar_t
#include <stddef.h>

/* 7.18.1.1  Exact-width integer types */
typedef signed char int8_t;
typedef unsigned char   uint8_t;
typedef short  int16_t;
typedef unsigned short  uint16_t;
typedef int  int32_t;
typedef unsigned   uint32_t;
typedef __int64  int64_t;
typedef unsigned __int64 uint64_t;

/* 7.18.1.2  Minimum-width integer types */
typedef signed char int_least8_t;
typedef unsigned char   uint_least8_t;
typedef short  int_least16_t;
typedef unsigned short  uint_least16_t;
typedef int  int_least32_t;
typedef unsigned   uint_least32_t;
typedef __int64  int_least64_t;
typedef unsigned __int64   uint_least64_t;

/*  7.18.1.3  Fastest minimum-width integer types 
 *  Not actually guaranteed to be fastest for all purposes
 *  Here we use the exact-width types for 8 and 16-bit ints. 
 */
typedef char int_fast8_t;
typedef unsigned char uint_fast8_t;
typedef short  int_fast16_t;
typedef unsigned short  uint_fast16_t;
typedef int  int_fast32_t;
typedef unsigned  int  uint_fast32_t;
typedef __int64  int_fast64_t;
typedef unsigned __int64   uint_fast64_t;

/* 7.18.1.4  Integer types capable of holding object pointers */
/*typedef int intptr_t;
typedef unsigned uintptr_t;*/

/* 7.18.1.5  Greatest-width integer types */
typedef __int64  intmax_t;
typedef unsigned __int64   uintmax_t;

/* 7.18.2  Limits of specified-width integer types */
#if !defined ( __cplusplus) || defined (__STDC_LIMIT_MACROS)

/* 7.18.2.1  Limits of exact-width integer types */
#define INT8_MIN (-128) 
#define INT16_MIN (-32768)
#define INT32_MIN (-2147483647 - 1)
#define INT64_MIN  (-9223372036854775807LL - 1)

#define INT8_MAX 127
#define INT16_MAX 32767
#define INT32_MAX 2147483647
#define INT64_MAX 9223372036854775807LL

#define UINT8_MAX 0xff /* 255U */
#define UINT16_MAX 0xffff /* 65535U */
#define UINT32_MAX 0xffffffff  /* 4294967295U */
#define UINT64_MAX 0xffffffffffffffffULL /* 18446744073709551615ULL */

/* 7.18.2.2  Limits of minimum-width integer types */
#define INT_LEAST8_MIN INT8_MIN
#define INT_LEAST16_MIN INT16_MIN
#define INT_LEAST32_MIN INT32_MIN
#define INT_LEAST64_MIN INT64_MIN

#define INT_LEAST8_MAX INT8_MAX
#define INT_LEAST16_MAX INT16_MAX
#define INT_LEAST32_MAX INT32_MAX
#define INT_LEAST64_MAX INT64_MAX

#define UINT_LEAST8_MAX UINT8_MAX
#define UINT_LEAST16_MAX UINT16_MAX
#define UINT_LEAST32_MAX UINT32_MAX
#define UINT_LEAST64_MAX UINT64_MAX

/* 7.18.2.3  Limits of fastest minimum-width integer types */
#define INT_FAST8_MIN INT8_MIN
#define INT_FAST16_MIN INT16_MIN
#define INT_FAST32_MIN INT32_MIN
#define INT_FAST64_MIN INT64_MIN

#define INT_FAST8_MAX INT8_MAX
#define INT_FAST16_MAX INT16_MAX
#define INT_FAST32_MAX INT32_MAX
#define INT_FAST64_MAX INT64_MAX

#define UINT_FAST8_MAX UINT8_MAX
#define UINT_FAST16_MAX UINT16_MAX
#define UINT_FAST32_MAX UINT32_MAX
#define UINT_FAST64_MAX UINT64_MAX

/* 7.18.2.4  Limits of integer types capable of holding
    object pointers */ 
#define INTPTR_MIN INT32_MIN
#define INTPTR_MAX INT32_MAX
#define UINTPTR_MAX UINT32_MAX

/* 7.18.2.5  Limits of greatest-width integer types */
#define INTMAX_MIN INT64_MIN
#define INTMAX_MAX INT64_MAX
#define UINTMAX_MAX UINT64_MAX

/* 7.18.3  Limits of other integer types */
#define PTRDIFF_MIN INT32_MIN
#define PTRDIFF_MAX INT32_MAX

#define SIG_ATOMIC_MIN INT32_MIN
#define SIG_ATOMIC_MAX INT32_MAX

#define SIZE_MAX UINT32_MAX

#ifndef WCHAR_MIN  /* also in wchar.h */ 
#define WCHAR_MIN 0
#define WCHAR_MAX ((wchar_t)-1) /* UINT16_MAX */
#endif

/*
 * wint_t is unsigned short for compatibility with MS runtime
 */
#define WINT_MIN 0
#define WINT_MAX ((wint_t)-1) /* UINT16_MAX */

#endif /* !defined ( __cplusplus) || defined __STDC_LIMIT_MACROS */


/* 7.18.4  Macros for integer constants */
#if !defined ( __cplusplus) || defined (__STDC_CONSTANT_MACROS)

/* 7.18.4.1  Macros for minimum-width integer constants

    Accoding to Douglas Gwyn <gwyn@arl.mil>:
	"This spec was changed in ISO/IEC 9899:1999 TC1; in ISO/IEC
	9899:1999 as initially published, the expansion was required
	to be an integer constant of precisely matching type, which
	is impossible to accomplish for the shorter types on most
	platforms, because C99 provides no standard way to designate
	an integer constant with width less than that of type int.
	TC1 changed this to require just an integer constant
	*expression* with *promoted* type."

	The trick used here is from Clive D W Feather.
*/

#define INT8_C(val) (INT_LEAST8_MAX-INT_LEAST8_MAX+(val))
#define INT16_C(val) (INT_LEAST16_MAX-INT_LEAST16_MAX+(val))
#define INT32_C(val) (INT_LEAST32_MAX-INT_LEAST32_MAX+(val))
#define INT64_C(val) (INT_LEAST64_MAX-INT_LEAST64_MAX+(val))

#define UINT8_C(val) (UINT_LEAST8_MAX-UINT_LEAST8_MAX+(val))
#define UINT16_C(val) (UINT_LEAST16_MAX-UINT_LEAST16_MAX+(val))
#define UINT32_C(val) (UINT_LEAST32_MAX-UINT_LEAST32_MAX+(val))
#define UINT64_C(val) (UINT_LEAST64_MAX-UINT_LEAST64_MAX+(val))

/* 7.18.4.2  Macros for greatest-width integer constants */
#define INTMAX_C(val) (INTMAX_MAX-INTMAX_MAX+(val))
#define UINTMAX_C(val) (UINTMAX_MAX-UINTMAX_MAX+(val))

#endif  /* !defined ( __cplusplus) || defined __STDC_CONSTANT_MACROS */

#endif
/*****************************************************************************
 * x264.h: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: x264.h,v 1.14 2004/05/09 20:29:34 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _X264_H
#define _X264_H 1

#define X264_BUILD 0x0006

/* x264_t:
 *      opaque handler for decoder and encoder */
typedef struct x264_t x264_t;

/****************************************************************************
 * Initialisation structure and function.
 ****************************************************************************/
/* CPU flags
 */
#define X264_CPU_MMX        0x000001    /* mmx */
#define X264_CPU_MMXEXT     0x000002    /* mmx-ext*/
#define X264_CPU_SSE        0x000004    /* sse */
#define X264_CPU_SSE2       0x000008    /* sse 2 */
#define X264_CPU_3DNOW      0x000010    /* 3dnow! */
#define X264_CPU_3DNOWEXT   0x000020    /* 3dnow! ext */
#define X264_CPU_ALTIVEC    0x000040    /* altivec */

/* Ananlyser flags
 * X264_ANALYSE_I4x4: analyse i4x4 modes
 * X264_ANALYSE_PSUB16x16: 16x8 and 8x16 and 8x8
 * X264_ANALYSE_PSUB8x8: 8x4 and 4x8 and 4x4
 */
#define X264_ANALYSE_I4x4       0x0001

#define X264_ANALYSE_PSUB16x16  0x0010
#define X264_ANALYSE_PSUB8x8    0x0020

typedef struct
{
    /* CPU flags */
    unsigned int cpu;

    /* Video Properties */
    int         i_width;
    int         i_height;

    struct
    {
        /* they will be reduced to be 0 < x <= 65535 and prime */
        int         i_sar_height;
        int         i_sar_width;
    } vui;

    float       f_fps;  /* Used for rate control only */

    /* Bitstream parameters */
    int         i_frame_reference;  /* Maximum number of reference frames */
    int         i_idrframe; /* every i_idrframe I frame are marked as IDR */
    int         i_iframe;   /* every i_iframe are intra */
    int         i_bframe;   /* how many b-frame between 2 references pictures */

    int         b_deblocking_filter;

    int         b_cabac;
    int         i_cabac_init_idc;

    int         i_qp_constant;  /* 1-51 */
    int         i_bitrate;      /* not working yet */

    /* Encoder analyser parameters */
    struct
    {
        unsigned int intra;     /* intra flags */
        unsigned int inter;     /* inter flags */
    } analyse;

} x264_param_t;

/* x264_param_default:
 *      fill x264_param_t with default values and do CPU detection */
void    x264_param_default( x264_param_t * );

/****************************************************************************
 * Picture structures and functions.
 ****************************************************************************/
typedef struct
{
    int     i_width;
    int     i_height;

    int     i_plane;
    int     i_stride[4];
    uint8_t *plane[4];
} x264_picture_t;

/* x264_picture_new:
 *      create a picture for the encoder */
x264_picture_t *x264_picture_new( x264_t * );

/* x264_picture_delete:
 *      destroy a x264_picture_t */
void            x264_picture_delete( x264_picture_t * );


/****************************************************************************
 * NAL structure and functions:
 ****************************************************************************/
/* nal */
enum nal_unit_type_e
{
    NAL_UNKNOWN = 0,
    NAL_SLICE   = 1,
    NAL_SLICE_DPA   = 2,
    NAL_SLICE_DPB   = 3,
    NAL_SLICE_DPC   = 4,
    NAL_SLICE_IDR   = 5,    /* ref_idc != 0 */
    NAL_SEI         = 6,    /* ref_idc == 0 */
    NAL_SPS         = 7,
    NAL_PPS         = 8
    /* ref_idc == 0 for 6,9,10,11,12 */
};
enum nal_priority_e
{
    NAL_PRIORITY_DISPOSABLE = 0,
    NAL_PRIORITY_LOW        = 1,
    NAL_PRIORITY_HIGH       = 2,
    NAL_PRIORITY_HIGHEST    = 3,
};

typedef struct
{
    int i_ref_idc;  /* nal_priority_e */
    int i_type;     /* nal_unit_type_e */

    /* This data are raw payload */
    int     i_payload;
    uint8_t *p_payload;
} x264_nal_t;

/* x264_nal_encode:
 *      encode a nal into a buffer, setting the size.
 *      if b_annexeb then a long synch work is added
 *      XXX: it currently doesn't check for overflow */
int x264_nal_encode( void *, int *, int b_annexeb, x264_nal_t *nal );

/* x264_nal_decode:
 *      decode a buffer nal into a x264_nal_t */
int x264_nal_decode( x264_nal_t *nal, void *, int );

/****************************************************************************
 * Encoder functions:
 ****************************************************************************/

/* x264_encoder_open:
 *      create a new encoder handler, all parameters from x264_param_t are copied */
x264_t *x264_encoder_open   ( x264_param_t * );
/* x264_encoder_headers:
 *      return the SPS and PPS that will be used for the whole stream */
int     x264_encoder_headers( x264_t *, x264_nal_t **, int * );
/* x264_encoder_encode:
 *      encode one picture.
 *      the picture *has* to be created by x264_picture_new */
int     x264_encoder_encode ( x264_t *, x264_nal_t **, int *, x264_picture_t * );
/* x264_encoder_close:
 *      close an encoder handler */
void    x264_encoder_close  ( x264_t * );

/* XXX: decoder isn't working so no need to export it */
#if 0
/****************************************************************************
 * Decoder functions:
 ****************************************************************************
 * XXX: Not yet working so do not try ...
 ****************************************************************************/
/* x264_decoder_open:
 */
x264_t *x264_decoder_open   ( x264_param_t * );
/* x264_decoder_decode:
 */
int     x264_decoder_decode ( x264_t *, x264_picture_t **, x264_nal_t * );
/* x264_decoder_close:
 */
void    x264_decoder_close  ( x264_t * );
#endif

/****************************************************************************
 * Private stuff for internal usage:
 ****************************************************************************/
#ifdef __X264__
#   ifdef _MSC_VER
#       define inline __inline
#       define DECLARE_ALIGNED( type, var, n ) __declspec(align(n)) type var
#   else
#       define DECLARE_ALIGNED( type, var, n ) type var __attribute__((aligned(n)))
#   endif
#endif

#endif
