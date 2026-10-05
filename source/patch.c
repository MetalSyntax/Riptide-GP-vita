/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <stdio.h>

#include <kubridge.h>
#include <so_util/so_util.h>

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#define FTELL sceLibcBridge_ftell
#define FSEEK sceLibcBridge_fseek
#else
#define FTELL ftell
#define FSEEK fseek
#endif

#include "utils/logger.h"

extern so_module so_mod;

/**
 * VuGenericFile::size(void*) inlines fileno() as `(short)fp->_file` (bionic
 * FILE layout, offset 0x0E) and calls fstat() on it. Our FILE* is not a bionic
 * one, so it always got fd 0 -> fstat fails -> size 0 -> the saved profile is
 * treated as empty and overwritten with a fresh one. Compute the size with
 * ftell/fseek instead.
 */
static int VuGenericFile_size(void * this, void * handle) {
    FILE * f = *(FILE **)handle;
    long cur = FTELL(f);
    if (cur < 0 || FSEEK(f, 0, SEEK_END) != 0)
        return 0;
    long size = FTELL(f);
    FSEEK(f, cur, SEEK_SET);
    l_debug("VuGenericFile::size(%p): %li", f, size);
    return size < 0 ? 0 : (int)size;
}

void so_patch(void) {
    hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN13VuGenericFile4sizeEPv"),
              (uintptr_t)&VuGenericFile_size);
}
