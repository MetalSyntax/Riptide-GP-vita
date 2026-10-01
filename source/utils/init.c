/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021-2022 Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/init.h"

#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "utils/settings.h"

#include <string.h>

#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/kernel/clib.h>
#include <psp2/power.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>
#include <fios/fios.h>

// Base addresses for the three Android .so files. Memory footprints
// (readelf -l, text + data/bss): libfmodex ~1.1 MiB, libfmodevent ~0.4 MiB,
// libBlue ~3.3 MiB -- 4 MiB apart leaves room for so_util's patch/cave areas.
#define LOAD_ADDRESS_FMODEX    0x98000000
#define LOAD_ADDRESS_FMODEVENT 0x98400000
#define LOAD_ADDRESS_BLUE      0x98800000

extern so_module so_mod;           // libBlue.so (engine, android_main)
extern so_module so_mod_fmodex;    // libfmodex.so
extern so_module so_mod_fmodevent; // libfmodevent.so (NEEDED libfmodex.so)

void java_overrides_init(void);

static void load_so(so_module *mod, const char *path, uintptr_t addr) {
    if (!file_exists(path)) {
        fatal_error("Looks like you haven't installed the data files for this "
                    "port, or they are in an incorrect location. Please make "
                    "sure that you have %s file exactly at that path.", path);
    }
    if (so_file_load(mod, path, addr) < 0) {
        l_fatal("%s could not be loaded.", path);
        fatal_error("Error: could not load %s.", path);
    }
    l_success("%s loaded at 0x%08X.", path, addr);
}

// Dependency order matters: so_resolve() falls back to already loaded
// modules, so libfmodex must be relocated/resolved before libfmodevent,
// and both before libBlue.
static void link_so(so_module *mod, const char *name) {
    so_relocate(mod);
    resolve_imports(mod);
    l_success("%s relocated and imports resolved.", name);
}

void soloader_init_all() {
	// Launch `app0:configurator.bin` on `-config` init param
    sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
    SceAppUtilAppEventParam eventParam;
    sceClibMemset(&eventParam, 0, sizeof(SceAppUtilAppEventParam));
    sceAppUtilReceiveAppEvent(&eventParam);
    if (eventParam.type == 0x05) {
        char buffer[2048];
        sceAppUtilAppEventParseLiveArea(&eventParam, buffer);
        if (strstr(buffer, "-config"))
            sceAppMgrLoadExec("app0:/configurator.bin", NULL, NULL);
    }

    // Set default overclock values
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

#ifdef USE_SCELIBC_IO
    if (fios_init(DATA_PATH) == 0)
        l_success("FIOS initialized.");
#endif

    if (!module_loaded("kubridge")) {
        l_fatal("kubridge is not loaded.");
        fatal_error("Error: kubridge.skprx is not installed.");
    }
    l_success("kubridge check passed.");

    load_so(&so_mod_fmodex, SO_PATH_FMODEX, LOAD_ADDRESS_FMODEX);
    load_so(&so_mod_fmodevent, SO_PATH_FMODEVENT, LOAD_ADDRESS_FMODEVENT);
    load_so(&so_mod, SO_PATH, LOAD_ADDRESS_BLUE);

    settings_load();
    l_success("Settings loaded.");

    link_so(&so_mod_fmodex, "libfmodex.so");
    link_so(&so_mod_fmodevent, "libfmodevent.so");
    link_so(&so_mod, "libBlue.so");

    so_patch();
    l_success("SO patched.");

    so_flush_caches(&so_mod_fmodex);
    so_flush_caches(&so_mod_fmodevent);
    so_flush_caches(&so_mod);
    l_success("SO caches flushed.");

    so_initialize(&so_mod_fmodex);
    so_initialize(&so_mod_fmodevent);
    so_initialize(&so_mod);
    l_success("SO initialized (init_array run for all three modules).");

    gl_preload();
    l_success("OpenGL preloaded.");

    jni_init();
    java_overrides_init();
    l_success("FalsoJNI initialized.");
}
