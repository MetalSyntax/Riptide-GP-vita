/**
 * @file  native_activity.c
 * @brief Stand-in for android.app.NativeActivity (see native_activity.h).
 */

#include "reimpl/native_activity.h"
#include "utils/logger.h"

#include <psp2/apputil.h>
#include <psp2/system_param.h>
#include <psp2/kernel/processmgr.h>

#include <stdlib.h>
#include <string.h>

#ifndef DATA_PATH_INT
#define DATA_PATH_INT DATA_PATH "files"
#endif

struct ANativeWindow { int32_t width, height, format; };
struct AConfiguration { char lang[2]; char country[2]; };

static ANativeActivityCallbacks g_callbacks;
static ANativeActivity g_activity;
static struct ANativeWindow g_window = { SCREEN_W, SCREEN_H, 1 /* RGBA_8888 */ };
static AInputQueue *g_input_queue = NULL;
static volatile int g_finished = 0;
static int g_paused = 0;

/* ---------------------------------------------------------------------------
 * Language
 * ------------------------------------------------------------------------ */

static int system_lang(void) {
    int lang = SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
    sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &lang);
    return lang;
}

const char *system_language_code(void) {
    switch (system_lang()) {
        case SCE_SYSTEM_PARAM_LANG_JAPANESE:      return "ja";
        case SCE_SYSTEM_PARAM_LANG_FRENCH:        return "fr";
        case SCE_SYSTEM_PARAM_LANG_SPANISH:       return "es";
        case SCE_SYSTEM_PARAM_LANG_GERMAN:        return "de";
        case SCE_SYSTEM_PARAM_LANG_ITALIAN:       return "it";
        case SCE_SYSTEM_PARAM_LANG_DUTCH:         return "nl";
        case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT:
        case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR: return "pt";
        case SCE_SYSTEM_PARAM_LANG_RUSSIAN:       return "ru";
        case SCE_SYSTEM_PARAM_LANG_KOREAN:        return "ko";
        case SCE_SYSTEM_PARAM_LANG_CHINESE_T:
        case SCE_SYSTEM_PARAM_LANG_CHINESE_S:     return "zh";
        case SCE_SYSTEM_PARAM_LANG_FINNISH:       return "fi";
        case SCE_SYSTEM_PARAM_LANG_SWEDISH:       return "sv";
        case SCE_SYSTEM_PARAM_LANG_DANISH:        return "da";
        case SCE_SYSTEM_PARAM_LANG_NORWEGIAN:     return "no";
        case SCE_SYSTEM_PARAM_LANG_POLISH:        return "pl";
        case SCE_SYSTEM_PARAM_LANG_TURKISH:       return "tr";
        default:                                  return "en";
    }
}

const char *system_country_code(void) {
    switch (system_lang()) {
        case SCE_SYSTEM_PARAM_LANG_JAPANESE:      return "JP";
        case SCE_SYSTEM_PARAM_LANG_FRENCH:        return "FR";
        case SCE_SYSTEM_PARAM_LANG_SPANISH:       return "ES";
        case SCE_SYSTEM_PARAM_LANG_GERMAN:        return "DE";
        case SCE_SYSTEM_PARAM_LANG_ITALIAN:       return "IT";
        case SCE_SYSTEM_PARAM_LANG_DUTCH:         return "NL";
        case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT: return "PT";
        case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR: return "BR";
        case SCE_SYSTEM_PARAM_LANG_RUSSIAN:       return "RU";
        case SCE_SYSTEM_PARAM_LANG_KOREAN:        return "KR";
        case SCE_SYSTEM_PARAM_LANG_CHINESE_T:     return "TW";
        case SCE_SYSTEM_PARAM_LANG_CHINESE_S:     return "CN";
        case SCE_SYSTEM_PARAM_LANG_FINNISH:       return "FI";
        case SCE_SYSTEM_PARAM_LANG_SWEDISH:       return "SE";
        case SCE_SYSTEM_PARAM_LANG_DANISH:        return "DK";
        case SCE_SYSTEM_PARAM_LANG_NORWEGIAN:     return "NO";
        case SCE_SYSTEM_PARAM_LANG_POLISH:        return "PL";
        case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB:    return "GB";
        case SCE_SYSTEM_PARAM_LANG_TURKISH:       return "TR";
        default:                                  return "US";
    }
}

/* ---------------------------------------------------------------------------
 * NDK functions
 * ------------------------------------------------------------------------ */

void ANativeActivity_finish(ANativeActivity *activity) {
    l_info("ANativeActivity_finish(%p): engine asked to quit", activity);
    g_finished = 1;
}

void ANativeActivity_setWindowFlags(ANativeActivity *activity, uint32_t addFlags,
                                    uint32_t removeFlags) {
    l_debug("ANativeActivity_setWindowFlags(add 0x%x, remove 0x%x)", addFlags, removeFlags);
}

int32_t ANativeWindow_getWidth(ANativeWindow *window) {
    return window ? window->width : SCREEN_W;
}

int32_t ANativeWindow_getHeight(ANativeWindow *window) {
    return window ? window->height : SCREEN_H;
}

int32_t ANativeWindow_getFormat(ANativeWindow *window) {
    return window ? window->format : 1;
}

int32_t ANativeWindow_setBuffersGeometry(ANativeWindow *window, int32_t width,
                                         int32_t height, int32_t format) {
    l_debug("ANativeWindow_setBuffersGeometry(%p, %i, %i, %i)", window, width,
            height, format);
    // The framebuffer is always 960x544 (vglInitExtended); 0x0 means "use
    // the window size", which is what we report anyway.
    if (window && format)
        window->format = format;
    return 0;
}

void ANativeWindow_acquire(ANativeWindow *window) {}
void ANativeWindow_release(ANativeWindow *window) {}

AConfiguration *AConfiguration_new(void) {
    return calloc(1, sizeof(AConfiguration));
}

void AConfiguration_delete(AConfiguration *config) {
    free(config);
}

void AConfiguration_fromAssetManager(AConfiguration *out, AAssetManager *am) {
    if (!out)
        return;
    memcpy(out->lang, system_language_code(), 2);
    memcpy(out->country, system_country_code(), 2);
}

void AConfiguration_getLanguage(AConfiguration *config, char *outLanguage) {
    if (!outLanguage)
        return;
    memcpy(outLanguage, config ? config->lang : "en", 2);
}

void AConfiguration_getCountry(AConfiguration *config, char *outCountry) {
    if (!outCountry)
        return;
    memcpy(outCountry, config ? config->country : "US", 2);
}

/* ---------------------------------------------------------------------------
 * Lifecycle driver
 * ------------------------------------------------------------------------ */

AInputQueue *native_activity_input_queue(void) {
    return g_input_queue;
}

int native_activity_finished(void) {
    return g_finished;
}

void native_activity_pause(void) {
    if (g_paused)
        return;
    g_paused = 1;
    if (g_callbacks.onWindowFocusChanged)
        g_callbacks.onWindowFocusChanged(&g_activity, 0);
    if (g_callbacks.onPause)
        g_callbacks.onPause(&g_activity);
}

void native_activity_resume(void) {
    if (!g_paused)
        return;
    g_paused = 0;
    if (g_callbacks.onResume)
        g_callbacks.onResume(&g_activity);
    if (g_callbacks.onWindowFocusChanged)
        g_callbacks.onWindowFocusChanged(&g_activity, 1);
}

int native_activity_bootstrap(so_module *mod) {
    void (*onCreate)(ANativeActivity *, void *, size_t) =
            (void *)so_symbol(mod, "ANativeActivity_onCreate");
    if (!onCreate) {
        l_fatal("ANativeActivity_onCreate not found in the .so");
        return -1;
    }

    memset(&g_callbacks, 0, sizeof(g_callbacks));
    memset(&g_activity, 0, sizeof(g_activity));
    g_activity.callbacks = &g_callbacks;
    g_activity.vm = &jvm;
    g_activity.env = &jni;
    g_activity.clazz = (jobject)strdup("com/vectorunit/blue/Blue");
    g_activity.internalDataPath = DATA_PATH_INT;
    g_activity.externalDataPath = DATA_PATH_INT;
    g_activity.sdkVersion = 19;
    g_activity.assetManager = AAssetManager_create();
    g_activity.obbPath = DATA_PATH "obb";

    l_info("native_activity: calling ANativeActivity_onCreate(%p)", onCreate);
    onCreate(&g_activity, NULL, 0);
    l_success("native_activity: ANativeActivity_onCreate returned, glue thread is up");

    if (g_callbacks.onStart) {
        g_callbacks.onStart(&g_activity);
        l_success("native_activity: onStart done");
    }
    if (g_callbacks.onResume) {
        g_callbacks.onResume(&g_activity);
        l_success("native_activity: onResume done");
    }

    g_input_queue = input_queue_create();
    if (g_callbacks.onInputQueueCreated) {
        g_callbacks.onInputQueueCreated(&g_activity, g_input_queue);
        l_success("native_activity: onInputQueueCreated done");
    }

    if (g_callbacks.onNativeWindowCreated) {
        g_callbacks.onNativeWindowCreated(&g_activity, &g_window);
        l_success("native_activity: onNativeWindowCreated done");
    }

    if (g_callbacks.onWindowFocusChanged) {
        g_callbacks.onWindowFocusChanged(&g_activity, 1);
        l_success("native_activity: onWindowFocusChanged(1) done");
    }

    return 0;
}
