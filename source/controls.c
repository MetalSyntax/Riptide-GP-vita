/**
 * @file  controls.c
 * @brief Vita touch + buttons -> engine input (see controls.h).
 */

#include "controls.h"
#include "reimpl/input_queue.h"
#include "reimpl/native_activity.h"
#include "utils/logger.h"

#include <psp2/ctrl.h>
#include <psp2/touch.h>

#include <falso_jni/FalsoJNI.h>

#include <math.h>
#include <string.h>

// Device id the Java MOGA listener always reported (VuGamePadHelper).
#define MOGA_DEVICE_ID 305419896

// MOGA button indices = Android KEYCODE_BUTTON_* - 96, DPAD = keycode - 19 + 15.
enum {
    MOGA_A = 0, MOGA_B = 1, MOGA_X = 3, MOGA_Y = 4,
    MOGA_L1 = 6, MOGA_R1 = 7, MOGA_L2 = 8, MOGA_R2 = 9,
    MOGA_THUMBL = 10, MOGA_THUMBR = 11,
    MOGA_START = 12, MOGA_SELECT = 13,
    MOGA_DPAD_UP = 15, MOGA_DPAD_DOWN = 16, MOGA_DPAD_LEFT = 17, MOGA_DPAD_RIGHT = 18,
};

static const struct { uint32_t vita; int moga; } button_map[] = {
    { SCE_CTRL_CROSS,    MOGA_A },
    { SCE_CTRL_CIRCLE,   MOGA_B },
    { SCE_CTRL_SQUARE,   MOGA_X },
    { SCE_CTRL_TRIANGLE, MOGA_Y },
    { SCE_CTRL_LTRIGGER, MOGA_L1 },
    { SCE_CTRL_RTRIGGER, MOGA_R1 },
    { SCE_CTRL_L2,       MOGA_L2 },   // PSTV / DS4 only
    { SCE_CTRL_R2,       MOGA_R2 },
    { SCE_CTRL_L3,       MOGA_THUMBL },
    { SCE_CTRL_R3,       MOGA_THUMBR },
    { SCE_CTRL_START,    MOGA_START },
    { SCE_CTRL_SELECT,   MOGA_SELECT },
    { SCE_CTRL_UP,       MOGA_DPAD_UP },
    { SCE_CTRL_DOWN,     MOGA_DPAD_DOWN },
    { SCE_CTRL_LEFT,     MOGA_DPAD_LEFT },
    { SCE_CTRL_RIGHT,    MOGA_DPAD_RIGHT },
};

static void (*onMogaButtonEvent)(JNIEnv *, jclass, jint, jint, jboolean);
static void (*onMogaAxisEvent)(JNIEnv *, jclass, jint, jfloat, jfloat, jfloat,
                               jfloat, jfloat, jfloat);

static uint32_t prev_buttons = 0;
static float prev_axes[6] = { 0 };

/* ---------------------------------------------------------------------------
 * Touch
 * ------------------------------------------------------------------------ */

typedef struct {
    int hw_id;     // SceTouchReport.id, only used to match reports across frames
    int ptr_id;    // small id handed to the engine (never the raw hw id)
    float x, y;
} touch_ptr;

static touch_ptr active[INPUT_MAX_POINTERS];
static int active_count = 0;

static int alloc_ptr_id(void) {
    for (int id = 0; id < INPUT_MAX_POINTERS; id++) {
        int used = 0;
        for (int i = 0; i < active_count; i++)
            if (active[i].ptr_id == id) used = 1;
        if (!used)
            return id;
    }
    return 0;
}

static void emit_motion(int action) {
    AInputEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = AINPUT_EVENT_TYPE_MOTION;
    ev.source = AINPUT_SOURCE_TOUCHSCREEN;
    ev.device_id = 1;
    ev.action = action;
    ev.pointer_count = active_count;
    for (int i = 0; i < active_count; i++) {
        ev.pointer_id[i] = active[i].ptr_id;
        ev.x[i] = active[i].x;
        ev.y[i] = active[i].y;
    }
    input_queue_push(native_activity_input_queue(), &ev);
}

static void poll_touch(void) {
    SceTouchData touch;
    if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) < 1)
        return;

    int n = touch.reportNum > INPUT_MAX_POINTERS ? INPUT_MAX_POINTERS : touch.reportNum;

    // 1) Released pointers: POINTER_UP (or UP for the last one). The event
    //    still lists the lifted pointer, like Android does.
    for (int i = 0; i < active_count;) {
        int still_down = 0;
        for (int r = 0; r < n; r++)
            if (touch.report[r].id == active[i].hw_id) still_down = 1;
        if (still_down) {
            i++;
            continue;
        }
        if (active_count == 1)
            emit_motion(AMOTION_EVENT_ACTION_UP);
        else
            emit_motion(AMOTION_EVENT_ACTION_POINTER_UP |
                        (i << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT));
        memmove(&active[i], &active[i + 1], (active_count - i - 1) * sizeof(touch_ptr));
        active_count--;
    }

    // 2) Moved / new pointers.
    int moved = 0;
    for (int r = 0; r < n; r++) {
        float x = touch.report[r].x * (float)SCREEN_W / 1920.0f;
        float y = touch.report[r].y * (float)SCREEN_H / 1088.0f;

        int idx = -1;
        for (int i = 0; i < active_count; i++)
            if (active[i].hw_id == touch.report[r].id) idx = i;

        if (idx >= 0) {
            if (active[idx].x != x || active[idx].y != y) {
                active[idx].x = x;
                active[idx].y = y;
                moved = 1;
            }
        } else if (active_count < INPUT_MAX_POINTERS) {
            active[active_count].hw_id = touch.report[r].id;
            active[active_count].ptr_id = alloc_ptr_id();
            active[active_count].x = x;
            active[active_count].y = y;
            active_count++;
            if (active_count == 1)
                emit_motion(AMOTION_EVENT_ACTION_DOWN);
            else
                emit_motion(AMOTION_EVENT_ACTION_POINTER_DOWN |
                            ((active_count - 1) << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT));
        }
    }

    if (moved)
        emit_motion(AMOTION_EVENT_ACTION_MOVE);
}

/* ---------------------------------------------------------------------------
 * Buttons / sticks -> MOGA
 * ------------------------------------------------------------------------ */

static float stick(uint8_t v) {
    float f = ((float)v - 127.5f) / 127.5f;
    if (fabsf(f) < 0.15f)
        return 0.0f;
    return f > 1.0f ? 1.0f : (f < -1.0f ? -1.0f : f);
}

static void poll_buttons(void) {
    SceCtrlData pad;
    if (sceCtrlPeekBufferPositiveExt2(0, &pad, 1) < 0)
        return;

    uint32_t changed = pad.buttons ^ prev_buttons;
    if (changed && onMogaButtonEvent) {
        for (unsigned i = 0; i < sizeof(button_map) / sizeof(button_map[0]); i++) {
            if (changed & button_map[i].vita)
                onMogaButtonEvent(&jni, NULL, MOGA_DEVICE_ID, button_map[i].moga,
                                  (pad.buttons & button_map[i].vita) ? JNI_TRUE : JNI_FALSE);
        }
    }
    prev_buttons = pad.buttons;

    // Axis order = VuGamePadHelper.onMotionEvent(): X, Y, Z, RZ, LTRIGGER, RTRIGGER.
    // The Vita has no analog triggers: mirror L/R so trigger-mapped actions
    // (throttle on most gamepad layouts) still work.
    float axes[6] = {
        stick(pad.lx), stick(pad.ly), stick(pad.rx), stick(pad.ry),
        (pad.buttons & (SCE_CTRL_LTRIGGER | SCE_CTRL_L2)) ? 1.0f : 0.0f,
        (pad.buttons & (SCE_CTRL_RTRIGGER | SCE_CTRL_R2)) ? 1.0f : 0.0f,
    };
    // Also resend once per second: the native drops events while
    // VuGamePad::mpInterface is still NULL (engine not initialised yet), and
    // the pad only gets registered on its first accepted event.
    static int resend = 0;
    if (onMogaAxisEvent && (memcmp(axes, prev_axes, sizeof(axes)) != 0 || ++resend >= 60)) {
        resend = 0;
        onMogaAxisEvent(&jni, NULL, MOGA_DEVICE_ID, axes[0], axes[1], axes[2],
                        axes[3], axes[4], axes[5]);
        memcpy(prev_axes, axes, sizeof(axes));
    }
}

void controls_init(so_module *mod) {
    sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
    // Touch sampling is off by default on the Vita.
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

    onMogaButtonEvent = (void *)so_symbol(mod, "Java_com_vectorunit_VuGamePadHelper_onMogaButtonEvent");
    onMogaAxisEvent = (void *)so_symbol(mod, "Java_com_vectorunit_VuGamePadHelper_onMogaAxisEvent");
    if (!onMogaButtonEvent || !onMogaAxisEvent)
        l_error("controls: MOGA natives not found, buttons will not work");
}

void controls_poll(void) {
    if (native_activity_input_queue())
        poll_touch();
    poll_buttons();
}
