/* libpng 1.6.22 CUSTOM API DEFINITION */

/* pnglibconf.h - library build configuration */

/* Libpng version 1.6.22 - May 29, 2016 */

/* Copyright (c) 1998-2015 Glenn Randers-Pehrson */

/* This code is released under the libpng license. */
/* For conditions of distribution and use, see the disclaimer */
/* and license in png.h */

/* pnglibconf.h */
/* Derived from: scripts/pnglibconf.dfa */
#ifndef PNGLCONF_H
#define PNGLCONF_H

/* default options */
/* These are PNG options that match the default in scripts/pnglibconf.dfa */
#define PNG_16BIT_SUPPORTED
#define PNG_ALIGNED_MEMORY_SUPPORTED
/*#undef PNG_ARM_NEON_API_SUPPORTED*/
/*#undef PNG_ARM_NEON_CHECK_SUPPORTED*/
#define PNG_BENIGN_ERRORS_SUPPORTED
#define PNG_BENIGN_READ_ERRORS_SUPPORTED
/*#undef PNG_BENIGN_WRITE_ERRORS_SUPPORTED*/
#define PNG_COLORSPACE_SUPPORTED
#define PNG_EASY_ACCESS_SUPPORTED
/*#undef PNG_ERROR_NUMBERS_SUPPORTED*/
#define PNG_ERROR_TEXT_SUPPORTED
#define PNG_FIXED_POINT_SUPPORTED
#define PNG_FLOATING_ARITHMETIC_SUPPORTED
#define PNG_FLOATING_POINT_SUPPORTED
#define PNG_FORMAT_AFIRST_SUPPORTED
#define PNG_FORMAT_BGR_SUPPORTED
#define PNG_GAMMA_SUPPORTED
#define PNG_HANDLE_AS_UNKNOWN_SUPPORTED
#define PNG_INFO_IMAGE_SUPPORTED
#define PNG_POINTER_INDEXING_SUPPORTED
#define PNG_PROGRESSIVE_READ_SUPPORTED
#define PNG_READ_16BIT_SUPPORTED
#define PNG_READ_ALPHA_MODE_SUPPORTED
#define PNG_READ_ANCILLARY_CHUNKS_SUPPORTED
#define PNG_READ_BACKGROUND_SUPPORTED
#define PNG_READ_BGR_SUPPORTED
#define PNG_READ_COMPOSITE_NODIV_SUPPORTED
#define PNG_READ_COMPRESSED_TEXT_SUPPORTED
#define PNG_READ_EXPAND_16_SUPPORTED
#define PNG_READ_EXPAND_SUPPORTED
#define PNG_READ_FILLER_SUPPORTED
#define PNG_READ_GAMMA_SUPPORTED
#define PNG_READ_GRAY_TO_RGB_SUPPORTED
#define PNG_READ_INTERLACING_SUPPORTED
#define PNG_READ_INT_FUNCTIONS_SUPPORTED
#define PNG_READ_PACKSWAP_SUPPORTED
#define PNG_READ_PACK_SUPPORTED
#define PNG_READ_RGB_TO_GRAY_SUPPORTED
#define PNG_READ_SCALE_16_TO_8_SUPPORTED
#define PNG_READ_SHIFT_SUPPORTED
#define PNG_READ_STRIP_16_TO_8_SUPPORTED
#define PNG_READ_STRIP_ALPHA_SUPPORTED
#define PNG_READ_SUPPORTED
#define PNG_READ_SWAP_ALPHA_SUPPORTED
#define PNG_READ_SWAP_SUPPORTED
#define PNG_READ_TEXT_SUPPORTED
#define PNG_READ_TRANSFORMS_SUPPORTED
#define PNG_READ_UNKNOWN_CHUNKS_SUPPORTED
#define PNG_READ_USER_CHUNKS_SUPPORTED
#define PNG_READ_USER_TRANSFORM_SUPPORTED
#define PNG_READ_cHRM_SUPPORTED
#define PNG_READ_gAMA_SUPPORTED
#define PNG_READ_iCCP_SUPPORTED
#define PNG_READ_sRGB_SUPPORTED
#define PNG_READ_tEXt_SUPPORTED
#define PNG_READ_tRNS_SUPPORTED
#define PNG_READ_zTXt_SUPPORTED
#define PNG_SAVE_INT_32_SUPPORTED
#define PNG_SAVE_UNKNOWN_CHUNKS_SUPPORTED
#define PNG_SEQUENTIAL_READ_SUPPORTED
#define PNG_SETJMP_SUPPORTED
#define PNG_SET_UNKNOWN_CHUNKS_SUPPORTED
#define PNG_SET_USER_LIMITS_SUPPORTED
#define PNG_SIMPLIFIED_READ_AFIRST_SUPPORTED
#define PNG_SIMPLIFIED_READ_BGR_SUPPORTED
#define PNG_SIMPLIFIED_READ_SUPPORTED
#define PNG_SIMPLIFIED_WRITE_AFIRST_SUPPORTED
#define PNG_SIMPLIFIED_WRITE_BGR_SUPPORTED
#define PNG_SIMPLIFIED_WRITE_STDIO_SUPPORTED
#define PNG_SIMPLIFIED_WRITE_SUPPORTED
#define PNG_STDIO_SUPPORTED
#define PNG_STORE_UNKNOWN_CHUNKS_SUPPORTED
#define PNG_TEXT_SUPPORTED
#define PNG_UNKNOWN_CHUNKS_SUPPORTED
#define PNG_USER_CHUNKS_SUPPORTED
#define PNG_USER_LIMITS_SUPPORTED
#define PNG_USER_MEM_SUPPORTED
#define PNG_USER_TRANSFORM_INFO_SUPPORTED
#define PNG_USER_TRANSFORM_PTR_SUPPORTED
#define PNG_WARNINGS_SUPPORTED
#define PNG_WRITE_16BIT_SUPPORTED
#define PNG_WRITE_ANCILLARY_CHUNKS_SUPPORTED
#define PNG_WRITE_BGR_SUPPORTED
#define PNG_WRITE_COMPRESSED_TEXT_SUPPORTED
#define PNG_WRITE_CUSTOMIZE_COMPRESSION_SUPPORTED
#define PNG_WRITE_CUSTOMIZE_ZTXT_COMPRESSION_SUPPORTED
#define PNG_WRITE_FILLER_SUPPORTED
#define PNG_WRITE_FILTER_SUPPORTED
#define PNG_WRITE_FLUSH_SUPPORTED
#define PNG_WRITE_INTERLACING_SUPPORTED
#define PNG_WRITE_INT_FUNCTIONS_SUPPORTED
#define PNG_WRITE_PACKSWAP_SUPPORTED
#define PNG_WRITE_PACK_SUPPORTED
#define PNG_WRITE_SHIFT_SUPPORTED
#define PNG_WRITE_SUPPORTED
#define PNG_WRITE_SWAP_ALPHA_SUPPORTED
#define PNG_WRITE_SWAP_SUPPORTED
#define PNG_WRITE_TEXT_SUPPORTED
#define PNG_WRITE_TRANSFORMS_SUPPORTED
#define PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED
#define PNG_WRITE_USER_TRANSFORM_SUPPORTED
#define PNG_WRITE_WEIGHTED_FILTER_SUPPORTED
#define PNG_WRITE_cHRM_SUPPORTED
#define PNG_WRITE_gAMA_SUPPORTED
#define PNG_WRITE_iCCP_SUPPORTED
#define PNG_WRITE_sRGB_SUPPORTED
#define PNG_WRITE_tEXt_SUPPORTED
#define PNG_WRITE_tRNS_SUPPORTED
#define PNG_WRITE_zTXt_SUPPORTED
#define PNG_cHRM_SUPPORTED
#define PNG_gAMA_SUPPORTED
#define PNG_iCCP_SUPPORTED
#define PNG_sBIT_SUPPORTED
#define PNG_sRGB_SUPPORTED
#define PNG_tEXt_SUPPORTED
#define PNG_tRNS_SUPPORTED
#define PNG_zTXt_SUPPORTED
/* end of options */

/* chromium options */
/* These are PNG options that chromium chooses to explicitly disable */
/*#undef PNG_BUILD_GRAYSCALE_PALETTE_SUPPORTED*/
/*#undef PNG_CHECK_FOR_INVALID_INDEX_SUPPORTED*/
/*#undef PNG_CONSOLE_IO_SUPPORTED*/
/*#undef PNG_CONVERT_tIME_SUPPORTED*/
/*#undef PNG_GET_PALETTE_MAX_SUPPORTED*/
/*#undef PNG_INCH_CONVERSIONS_SUPPORTED*/
/*#undef PNG_IO_STATE_SUPPORTED*/
/*#undef PNG_MNG_FEATURES_SUPPORTED*/
/*#undef PNG_READ_CHECK_FOR_INVALID_INDEX_SUPPORTED*/
/*#undef PNG_READ_GET_PALETTE_MAX_SUPPORTED*/
/*#undef PNG_READ_INVERT_ALPHA_SUPPORTED*/
/*#undef PNG_READ_INVERT_SUPPORTED*/
/*#undef PNG_READ_OPT_PLTE_SUPPORTED*/
/*#undef PNG_READ_QUANTIZE_SUPPORTED*/
/*#undef PNG_READ_bKGD_SUPPORTED*/
/*#undef PNG_READ_hIST_SUPPORTED*/
/*#undef PNG_READ_iTXt_SUPPORTED*/
/*#undef PNG_READ_oFFs_SUPPORTED*/
/*#undef PNG_READ_pCAL_SUPPORTED*/
/*#undef PNG_READ_pHYs_SUPPORTED*/
/*#undef PNG_READ_sBIT_SUPPORTED*/
/*#undef PNG_READ_sCAL_SUPPORTED*/
/*#undef PNG_READ_sPLT_SUPPORTED*/
/*#undef PNG_READ_tIME_SUPPORTED*/
/*#undef PNG_SET_OPTION_SUPPORTED*/
/*#undef PNG_TIME_RFC1123_SUPPORTED*/
/*#undef PNG_WRITE_CHECK_FOR_INVALID_INDEX_SUPPORTED*/
/*#undef PNG_WRITE_GET_PALETTE_MAX_SUPPORTED*/
/*#undef PNG_WRITE_INVERT_ALPHA_SUPPORTED*/
/*#undef PNG_WRITE_INVERT_SUPPORTED*/
/*#undef PNG_WRITE_OPTIMIZE_CMF_SUPPORTED*/
/*#undef PNG_WRITE_bKGD_SUPPORTED*/
/*#undef PNG_WRITE_hIST_SUPPORTED*/
/*#undef PNG_WRITE_iTXt_SUPPORTED*/
/*#undef PNG_WRITE_oFFs_SUPPORTED*/
/*#undef PNG_WRITE_pCAL_SUPPORTED*/
/*#undef PNG_WRITE_pHYs_SUPPORTED*/
/*#undef PNG_WRITE_sBIT_SUPPORTED*/
/*#undef PNG_WRITE_sCAL_SUPPORTED*/
/*#undef PNG_WRITE_sPLT_SUPPORTED*/
/*#undef PNG_WRITE_tIME_SUPPORTED*/
/*#undef PNG_bKGD_SUPPORTED*/
/*#undef PNG_hIST_SUPPORTED*/
/*#undef PNG_iTXt_SUPPORTED*/
/*#undef PNG_oFFs_SUPPORTED*/
/*#undef PNG_pCAL_SUPPORTED*/
/*#undef PNG_pHYs_SUPPORTED*/
/*#undef PNG_sCAL_SUPPORTED*/
/*#undef PNG_sPLT_SUPPORTED*/
/*#undef PNG_tIME_SUPPORTED*/
/* end of chromium options */

/* default settings */
/* These are PNG settings that match the default in scripts/pnglibconf.dfa */
#define PNG_API_RULE 0
#define PNG_DEFAULT_READ_MACROS 1
#define PNG_GAMMA_THRESHOLD_FIXED 5000
#define PNG_IDAT_READ_SIZE PNG_ZBUF_SIZE
#define PNG_INFLATE_BUF_SIZE 1024
#define PNG_LINKAGE_API extern
#define PNG_LINKAGE_CALLBACK extern
#define PNG_LINKAGE_DATA extern
#define PNG_LINKAGE_FUNCTION extern
#define PNG_MAX_GAMMA_8 11
#define PNG_QUANTIZE_BLUE_BITS 5
#define PNG_QUANTIZE_GREEN_BITS 5
#define PNG_QUANTIZE_RED_BITS 5
#define PNG_TEXT_Z_DEFAULT_COMPRESSION (-1)
#define PNG_TEXT_Z_DEFAULT_STRATEGY 0
#define PNG_USER_HEIGHT_MAX 1000000
#define PNG_USER_WIDTH_MAX 1000000
#define PNG_ZBUF_SIZE 8192
#define PNG_ZLIB_VERNUM 0 /* unknown */
#define PNG_Z_DEFAULT_COMPRESSION (-1)
#define PNG_Z_DEFAULT_NOFILTER_STRATEGY 0
#define PNG_Z_DEFAULT_STRATEGY 1
#define PNG_sCAL_PRECISION 5
#define PNG_sRGB_PROFILE_CHECKS 2
/* end of default settings */

/* chromium settings */
/* These are PNG setting that chromium has modified */
/* crbug.com/117369 */
#define PNG_USER_CHUNK_CACHE_MAX 128
#define PNG_USER_CHUNK_MALLOC_MAX 4000000L
/* end of chromium settings */

#endif /* PNGLCONF_H */
