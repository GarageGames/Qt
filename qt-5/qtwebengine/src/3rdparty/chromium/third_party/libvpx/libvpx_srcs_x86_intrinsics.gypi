# This file is generated. Do not edit.
# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libvpx_intrinsics_mmx',
      'type': 'static_library',
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        '<(libvpx_source)',
      ],
      'sources': [
        '<(libvpx_source)/vp8/common/x86/idct_blk_mmx.c',
        '<(libvpx_source)/vp8/common/x86/variance_mmx.c',
        '<(libvpx_source)/vp8/encoder/x86/vp8_enc_stubs_mmx.c',
      ],
      'cflags': [ '-mmmx', ],
      'xcode_settings': { 'OTHER_CFLAGS': [ '-mmmx' ] },
    },
    {
      'target_name': 'libvpx_intrinsics_sse2',
      'type': 'static_library',
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        '<(libvpx_source)',
      ],
      'sources': [
        '<(libvpx_source)/vp8/common/x86/idct_blk_sse2.c',
        '<(libvpx_source)/vp8/common/x86/recon_wrapper_sse2.c',
        '<(libvpx_source)/vp8/common/x86/variance_sse2.c',
        '<(libvpx_source)/vp8/encoder/x86/denoising_sse2.c',
        '<(libvpx_source)/vp8/encoder/x86/quantize_sse2.c',
        '<(libvpx_source)/vp8/encoder/x86/vp8_enc_stubs_sse2.c',
        '<(libvpx_source)/vp9/common/x86/vp9_idct_intrin_sse2.c',
        '<(libvpx_source)/vp9/common/x86/vp9_loopfilter_intrin_sse2.c',
        '<(libvpx_source)/vp9/encoder/x86/vp9_avg_intrin_sse2.c',
        '<(libvpx_source)/vp9/encoder/x86/vp9_dct32x32_sse2.c',
        '<(libvpx_source)/vp9/encoder/x86/vp9_dct_sse2.c',
        '<(libvpx_source)/vp9/encoder/x86/vp9_quantize_sse2.c',
        '<(libvpx_source)/vp9/encoder/x86/vp9_variance_sse2.c',
      ],
      'cflags': [ '-msse2', ],
      'xcode_settings': { 'OTHER_CFLAGS': [ '-msse2' ] },
    },
    {
      'target_name': 'libvpx_intrinsics_ssse3',
      'type': 'static_library',
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        '<(libvpx_source)',
      ],
      'sources': [
        '<(libvpx_source)/vp8/common/x86/variance_ssse3.c',
        '<(libvpx_source)/vp8/encoder/x86/quantize_ssse3.c',
        '<(libvpx_source)/vp9/common/x86/vp9_idct_intrin_ssse3.c',
        '<(libvpx_source)/vp9/common/x86/vp9_subpixel_8t_intrin_ssse3.c',
      ],
      'cflags': [ '-mssse3', ],
      'xcode_settings': { 'OTHER_CFLAGS': [ '-mssse3' ] },
      'conditions': [
        ['OS=="win" and clang==1', {
          # cl.exe's /arch flag doesn't have a setting for SSSE3/4, and cl.exe
          # doesn't need it for intrinsics. clang-cl does need it, though.
          'msvs_settings': {
            'VCCLCompilerTool': { 'AdditionalOptions': [ '-mssse3' ] },
          },
        }],
      ],
    },
    {
      'target_name': 'libvpx_intrinsics_sse4_1',
      'type': 'static_library',
      'include_dirs': [
        'source/config/<(OS_CATEGORY)/<(target_arch_full)',
        '<(libvpx_source)',
      ],
      'sources': [
        '<(libvpx_source)/vp8/encoder/x86/quantize_sse4.c',
      ],
      'cflags': [ '-msse4.1', ],
      'xcode_settings': { 'OTHER_CFLAGS': [ '-msse4.1' ] },
      'conditions': [
        ['OS=="win" and clang==1', {
          # cl.exe's /arch flag doesn't have a setting for SSSE3/4, and cl.exe
          # doesn't need it for intrinsics. clang-cl does need it, though.
          'msvs_settings': {
            'VCCLCompilerTool': { 'AdditionalOptions': [ '-msse4.1' ] },
          },
        }],
      ],
    },
  ],
}
