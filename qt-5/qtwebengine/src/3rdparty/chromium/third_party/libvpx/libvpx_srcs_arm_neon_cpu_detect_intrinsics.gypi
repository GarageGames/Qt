# This file is generated. Do not edit.
# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libvpx_intrinsics_neon',
      'type': 'static_library',
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        '<(libvpx_source)',
      ],
      'sources': [
        '<(libvpx_source)/vp8/common/arm/neon/bilinearpredict_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/copymem_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/dc_only_idct_add_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/dequant_idct_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/dequantizeb_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/idct_blk_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/idct_dequant_0_2x_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/idct_dequant_full_2x_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/iwalsh_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/loopfilter_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/loopfiltersimplehorizontaledge_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/loopfiltersimpleverticaledge_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/mbloopfilter_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/reconintra_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/sad_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/shortidct4x4llm_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/sixtappredict_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/variance_neon.c',
        '<(libvpx_source)/vp8/common/arm/neon/vp8_subpixelvariance_neon.c',
        '<(libvpx_source)/vp8/encoder/arm/neon/denoising_neon.c',
        '<(libvpx_source)/vp8/encoder/arm/neon/shortfdct_neon.c',
        '<(libvpx_source)/vp8/encoder/arm/neon/subtract_neon.c',
        '<(libvpx_source)/vp8/encoder/arm/neon/vp8_mse16x16_neon.c',
        '<(libvpx_source)/vp8/encoder/arm/neon/vp8_shortwalsh4x4_neon.c',
        '<(libvpx_source)/vp9/common/arm/neon/vp9_convolve_neon.c',
        '<(libvpx_source)/vp9/common/arm/neon/vp9_idct16x16_neon.c',
        '<(libvpx_source)/vp9/common/arm/neon/vp9_loopfilter_16_neon.c',
        '<(libvpx_source)/vp9/encoder/arm/neon/vp9_dct_neon.c',
        '<(libvpx_source)/vp9/encoder/arm/neon/vp9_quantize_neon.c',
        '<(libvpx_source)/vp9/encoder/arm/neon/vp9_sad_neon.c',
        '<(libvpx_source)/vp9/encoder/arm/neon/vp9_subtract_neon.c',
        '<(libvpx_source)/vp9/encoder/arm/neon/vp9_variance_neon.c',
      ],
      'cflags!': [ '-mfpu=vfpv3-d16' ],
      'conditions': [
        # Disable LTO in neon targets due to compiler bug
        # crbug.com/408997
        ['use_lto==1', {
          'cflags!': [
            '-flto',
            '-ffat-lto-objects',
          ],
        }],
      ],
      'cflags': [ '-mfpu=neon', ],
      'xcode_settings': { 'OTHER_CFLAGS': [ '-mfpu=neon' ] },
    },
  ],
}
