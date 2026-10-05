/**
 * @file  sensor.c
 * @brief Accelerometer for the engine, backed by sceMotion.
 *
 * Display.getRotation() reports ROTATION_0 (java.c), so the engine takes the
 * samples as already screen-aligned: Android +X right, +Y up, +Z out of the
 * screen, reporting the reaction to gravity (+g on Z lying face up). SceMotion
 * uses the same axes but reports the gravity vector itself, hence the sign
 * flip. `accelerometer 0` in config.txt reports a constant "lying flat"
 * vector instead (no tilt); `invert_tilt 1` flips left/right.
 */

#include "reimpl/sensor.h"
#include "utils/logger.h"
#include "utils/settings.h"

#include <psp2/motion.h>
#include <psp2/kernel/processmgr.h>

#include <pthread.h>
#include <string.h>

struct ASensorManager { int dummy; };
struct ASensor { int type; };
struct ASensorEventQueue {
    ALooper *looper;
    int enabled;
    uint32_t period_us;
    uint64_t last_us;
};

static struct ASensorManager g_manager;
static struct ASensor g_accel = { ASENSOR_TYPE_ACCELEROMETER };
static struct ASensorEventQueue g_queue;
static int g_motion_started = 0;

int sensor_queue_ready(void *q) {
    struct ASensorEventQueue *queue = q;
    if (!queue || !queue->enabled)
        return 0;
    return sceKernelGetProcessTimeWide() - queue->last_us >= queue->period_us;
}

ASensorManager *ASensorManager_getInstance(void) {
    return &g_manager;
}

const ASensor *ASensorManager_getDefaultSensor(ASensorManager *manager, int type) {
    l_debug("ASensorManager_getDefaultSensor(%i)", type);
    if (type == ASENSOR_TYPE_ACCELEROMETER)
        return &g_accel;
    return NULL;
}

ASensorEventQueue *ASensorManager_createEventQueue(ASensorManager *manager,
                                                   ALooper *looper, int ident,
                                                   ALooper_callbackFunc callback,
                                                   void *data) {
    l_debug("ASensorManager_createEventQueue(looper %p, ident %i)", looper, ident);
    memset(&g_queue, 0, sizeof(g_queue));
    g_queue.looper = looper;
    g_queue.period_us = 1000000 / 60;
    looper_attach_source(looper, LOOPER_SRC_SENSOR, &g_queue, ident, data);
    return &g_queue;
}

int ASensorManager_destroyEventQueue(ASensorManager *manager, ASensorEventQueue *queue) {
    looper_detach_source(queue ? queue->looper : NULL, queue);
    return 0;
}

int ASensorEventQueue_enableSensor(ASensorEventQueue *queue, const ASensor *sensor) {
    l_debug("ASensorEventQueue_enableSensor(%p, %p)", queue, sensor);
    if (!queue || !sensor)
        return -1;
    if (setting_accelerometer && !g_motion_started) {
        int ret = sceMotionStartSampling();
        g_motion_started = ret >= 0;
        l_info("sceMotionStartSampling: 0x%08x", ret);
    }
    queue->enabled = 1;
    queue->last_us = sceKernelGetProcessTimeWide();
    return 0;
}

int ASensorEventQueue_disableSensor(ASensorEventQueue *queue, const ASensor *sensor) {
    l_debug("ASensorEventQueue_disableSensor(%p, %p)", queue, sensor);
    if (queue)
        queue->enabled = 0;
    return 0;
}

int ASensorEventQueue_setEventRate(ASensorEventQueue *queue, const ASensor *sensor,
                                   int32_t usec) {
    l_debug("ASensorEventQueue_setEventRate(%p, %i us)", queue, usec);
    if (!queue)
        return -1;
    // Never deliver faster than 60 Hz: the engine drains all due events per
    // frame and anything faster only burns CPU.
    queue->period_us = usec < 16666 ? 16666 : (uint32_t)usec;
    return 0;
}

ssize_t ASensorEventQueue_getEvents(ASensorEventQueue *queue, ASensorEvent *events,
                                    size_t count) {
    if (!queue || !events || count == 0 || !sensor_queue_ready(queue))
        return 0;

    uint64_t now = sceKernelGetProcessTimeWide();
    queue->last_us = now;

    memset(&events[0], 0, sizeof(ASensorEvent));
    events[0].version = sizeof(ASensorEvent);
    events[0].sensor = 0;
    events[0].type = ASENSOR_TYPE_ACCELEROMETER;
    events[0].timestamp = (int64_t)now * 1000;

    SceMotionSensorState st;
    if (g_motion_started && sceMotionGetSensorState(&st, 1) >= 0) {
        float x = -st.accelerometer.x * ASENSOR_STANDARD_GRAVITY;
        if (setting_invertTilt)
            x = -x;
        events[0].acceleration.x = x;
        events[0].acceleration.y = -st.accelerometer.y * ASENSOR_STANDARD_GRAVITY;
        events[0].acceleration.z = -st.accelerometer.z * ASENSOR_STANDARD_GRAVITY;

        static int logged = 0;
        if (!logged) {
            logged = 1;
            l_info("accel first sample: vita g=(%.2f, %.2f, %.2f) -> android (%.2f, %.2f, %.2f)",
                   st.accelerometer.x, st.accelerometer.y, st.accelerometer.z,
                   events[0].acceleration.x, events[0].acceleration.y,
                   events[0].acceleration.z);
        }
        return 1;
    }
    events[0].acceleration.x = 0.0f;
    events[0].acceleration.y = 0.0f;
    events[0].acceleration.z = ASENSOR_STANDARD_GRAVITY;
    return 1;
}
