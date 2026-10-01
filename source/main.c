/**
 * @file  main.c
 * @brief Riptide GP (com.vectorunit.blue) loader entry point.
 *
 * libBlue.so is a NativeActivity app with android_native_app_glue linked in
 * statically (exports ANativeActivity_onCreate + android_main, no JNI_OnLoad).
 * The main thread here plays the role of the Android UI thread:
 *   1. load/link libfmodex -> libfmodevent -> libBlue (utils/init.c),
 *   2. Blue.onCreate(): setInternalDataPath(getFilesDir()) + ANativeActivity
 *      lifecycle (reimpl/native_activity.c), which spawns the glue thread
 *      that runs android_main -> OnInitApp / OnInitWindow / OnStep,
 *   3. Blue.onStart(): FMODAudioDevice.start() (reimpl/audio.c, started
 *      first so it is already pumping when FMOD initialises),
 *   4. forward touch/buttons to the engine forever (controls.c).
 */

#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/dialog.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/stat.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include "controls.h"
#include "reimpl/audio.h"
#include "reimpl/native_activity.h"

int _newlib_heap_size_user = 256 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod;            // libBlue.so
so_module so_mod_fmodex;     // libfmodex.so
so_module so_mod_fmodevent;  // libfmodevent.so

int main() {
    // Android's getFilesDir() and the obb dir must exist before the engine
    // touches them (VuAndroidFile::setInternalDataPath just stores the path).
    sceIoMkdir(DATA_PATH "files", 0777);
    sceIoMkdir(DATA_PATH "obb", 0777);

    soloader_init_all();

    gl_init();
    l_success("vitaGL initialized.");

    void (*setInternalDataPath)(JNIEnv *, jclass, jstring) =
            (void *)so_symbol(&so_mod, "Java_com_vectorunit_blue_Blue_setInternalDataPath");
    if (setInternalDataPath) {
        setInternalDataPath(&jni, NULL, jni->NewStringUTF(&jni, DATA_PATH "files"));
        l_success("Blue.setInternalDataPath(\"%s\") done.", DATA_PATH "files");
    } else {
        l_error("Java_com_vectorunit_blue_Blue_setInternalDataPath not found");
    }

    controls_init(&so_mod);

    // FMODAudioDevice.start() (Blue.onStart()). Started before the lifecycle
    // walk because onStart blocks until android_main has gone through
    // OnInitApp (VuAudio::init); the pump thread just idles until FMOD's
    // mixer reports a sample rate, so it never waits on the engine.
    audio_start();

    if (native_activity_bootstrap(&so_mod) < 0)
        fatal_error("Error: libBlue.so has no ANativeActivity_onCreate.");

    while (!native_activity_finished()) {
        controls_poll();
        sceKernelDelayThread(16666);
    }

    l_info("Engine finished, exiting.");
    audio_stop();
    sceKernelExitProcess(0);
    return 0;
}
