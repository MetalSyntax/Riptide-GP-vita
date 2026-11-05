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
#include "trophies.h"

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

/**
 * Trophies. Every achievement change in the engine goes through
 * VuAchievementManager::updateProgression(name, percent) (setCompleted and
 * setTaskCompleted tail-call it), keyed by the AchievementDB name. After it
 * runs, re-check every achievement with the engine's own
 * isAchievementUnlocked() and queue the matching trophy: this also catches
 * achievements earned in older saves (the trophy API ignores repeats).
 * Index = Vita trophy id; order must match extras/scripts/generate_trophies.py.
 * "Beat a friend's leaderboard time" (needs Google Play) has no trophy.
 */
static const char *const ACHIEVEMENTS[] = {
    NULL, /* 0 = Platinum, unlocked by the OS */
    "Earn a medal", "Earn a gold medal", "Earn a 1000cc gold medal",
    "Unlock a jet ski", "Unlock all jet skis",
    "Medal in all race events", "Medal in all hot lap events",
    "Medal in all champ events", "Medal in all events",
    "Gold in all race events", "Gold in all hot lap events",
    "Gold in all champ events", "Gold in all events",
    "1000cc Gold in all race events", "1000cc Gold in all hot lap events",
    "1000cc Gold in all champ events", "1000cc Gold in all events",
    "Earn a medal in Champ A", "Earn a medal in Champ B", "Earn a medal in Champ C",
    "Earn a medal in Champ D", "Earn a medal in Champ E",
    "Perform a stunt", "Perform all stunts",
    "Launch the rocket", "Break the bridge", "Harpist", "Props", "Luddite",
};
#define NUM_ACHIEVEMENTS (sizeof(ACHIEVEMENTS) / sizeof(ACHIEVEMENTS[0]))

static so_hook updateProgression_hook;
static int (*isAchievementUnlocked)(void *, const char *);
static uint32_t trophies_queued; /* bit N = trophy N already sent this session */

static void updateProgression(void *this, const char *name, float percent) {
    // int: SO_CONTINUE needs a non-void type; the real function returns void.
    SO_CONTINUE(int, updateProgression_hook, this, name, percent);
    for (uint32_t id = 1; id < NUM_ACHIEVEMENTS; id++) {
        if (!(trophies_queued & (1u << id)) && isAchievementUnlocked(this, ACHIEVEMENTS[id])) {
            trophies_queued |= 1u << id;
            l_info("Achievement \"%s\" -> trophy %u", ACHIEVEMENTS[id], (unsigned)id);
            trophies_unlock(id);
        }
    }
}

void so_patch(void) {
    hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN13VuGenericFile4sizeEPv"),
              (uintptr_t)&VuGenericFile_size);

    isAchievementUnlocked = (void *)so_symbol(&so_mod, "_ZN20VuAchievementManager21isAchievementUnlockedEPKc");
    updateProgression_hook = hook_addr(
            (uintptr_t)so_symbol(&so_mod, "_ZN20VuAchievementManager17updateProgressionEPKcf"),
            (uintptr_t)&updateProgression);
}
