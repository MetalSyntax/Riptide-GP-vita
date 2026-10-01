/**
 * @file  native_activity.h
 * @brief ANativeActivity / ANativeWindow / AConfiguration replacement and the
 *        lifecycle driver that stands in for android.app.NativeActivity.
 *
 * Layouts follow the public NDK headers (android/native_activity.h). The
 * callback offsets were cross-checked against the stores done by the
 * statically linked glue in libBlue.so (ANativeActivity_onCreate @ 0x110410):
 * +0 onStart, +4 onResume, +8 onSaveInstanceState, +12 onPause, +16 onStop,
 * +20 onDestroy, +24 onWindowFocusChanged, +28 onNativeWindowCreated,
 * +40 onNativeWindowDestroyed, +44 onInputQueueCreated,
 * +48 onInputQueueDestroyed, +56 onConfigurationChanged, +60 onLowMemory.
 */

#ifndef SOLOADER_NATIVE_ACTIVITY_H
#define SOLOADER_NATIVE_ACTIVITY_H

#include <stdint.h>
#include <stddef.h>
#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include "reimpl/asset_manager.h"
#include "reimpl/input_queue.h"

#define SCREEN_W 960
#define SCREEN_H 544

typedef struct ANativeWindow ANativeWindow;
typedef struct AConfiguration AConfiguration;
typedef struct ANativeActivity ANativeActivity;

typedef struct ANativeActivityCallbacks {
    void (*onStart)(ANativeActivity *activity);
    void (*onResume)(ANativeActivity *activity);
    void *(*onSaveInstanceState)(ANativeActivity *activity, size_t *outSize);
    void (*onPause)(ANativeActivity *activity);
    void (*onStop)(ANativeActivity *activity);
    void (*onDestroy)(ANativeActivity *activity);
    void (*onWindowFocusChanged)(ANativeActivity *activity, int hasFocus);
    void (*onNativeWindowCreated)(ANativeActivity *activity, ANativeWindow *window);
    void (*onNativeWindowResized)(ANativeActivity *activity, ANativeWindow *window);
    void (*onNativeWindowRedrawNeeded)(ANativeActivity *activity, ANativeWindow *window);
    void (*onNativeWindowDestroyed)(ANativeActivity *activity, ANativeWindow *window);
    void (*onInputQueueCreated)(ANativeActivity *activity, AInputQueue *queue);
    void (*onInputQueueDestroyed)(ANativeActivity *activity, AInputQueue *queue);
    void (*onContentRectChanged)(ANativeActivity *activity, const void *rect);
    void (*onConfigurationChanged)(ANativeActivity *activity);
    void (*onLowMemory)(ANativeActivity *activity);
} ANativeActivityCallbacks;

struct ANativeActivity {
    ANativeActivityCallbacks *callbacks;
    JavaVM *vm;
    JNIEnv *env;
    jobject clazz;
    const char *internalDataPath;
    const char *externalDataPath;
    int32_t sdkVersion;
    void *instance;
    AAssetManager *assetManager;
    const char *obbPath;
};

/**
 * Builds the fake activity, calls the .so's ANativeActivity_onCreate and
 * then walks the usual onStart -> onResume -> onInputQueueCreated ->
 * onNativeWindowCreated -> onWindowFocusChanged(1) sequence.
 * @return 0 on success, <0 if the .so is missing the entry point.
 */
int native_activity_bootstrap(so_module *mod);

/** The queue handed to the glue; the controls thread pushes into it. */
AInputQueue *native_activity_input_queue(void);

/** Set when the engine calls ANativeActivity_finish(). */
int native_activity_finished(void);

/** Called on PS button / suspend: onPause + focus lost, and the reverse. */
void native_activity_pause(void);
void native_activity_resume(void);

/* NDK functions imported by libBlue.so */
void ANativeActivity_finish(ANativeActivity *activity);
void ANativeActivity_setWindowFlags(ANativeActivity *activity, uint32_t addFlags,
                                    uint32_t removeFlags);

int32_t ANativeWindow_getWidth(ANativeWindow *window);
int32_t ANativeWindow_getHeight(ANativeWindow *window);
int32_t ANativeWindow_getFormat(ANativeWindow *window);
int32_t ANativeWindow_setBuffersGeometry(ANativeWindow *window, int32_t width,
                                         int32_t height, int32_t format);
void ANativeWindow_acquire(ANativeWindow *window);
void ANativeWindow_release(ANativeWindow *window);

AConfiguration *AConfiguration_new(void);
void AConfiguration_delete(AConfiguration *config);
void AConfiguration_fromAssetManager(AConfiguration *out, AAssetManager *am);
void AConfiguration_getLanguage(AConfiguration *config, char *outLanguage);
void AConfiguration_getCountry(AConfiguration *config, char *outCountry);

/** Two-letter ISO codes derived from the console's system language. */
const char *system_language_code(void);
const char *system_country_code(void);

#endif // SOLOADER_NATIVE_ACTIVITY_H
