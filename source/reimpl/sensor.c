/**
 * @file  sensor.c
 * @brief Accelerometer for the engine, backed by sceMotion.
 *
 * Tilt steering is only one of the game's control schemes; with the MOGA
 * pad path (see source/controls.c) the analog stick drives the boat. Until the
 * axis mapping is confirmed on real hardware, SENSOR_USE_MOTION stays 0 and a
 * constant "device lying flat" gravity vector is reported, so the engine gets
 * well-formed samples but no phantom steering.
 */

#include "reimpl/sensor.h"
#include "utils/logger.h"

#include <psp2/motion.h>
#include <psp2/kernel/processmgr.h>

#include <pthread.h>
#include <string.h>

#ifndef SENSOR_USE_MOTION
#define SENSOR_USE_MOTION 0
#endif

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
#if SENSOR_USE_MOTION
    if (!g_motion_started) {
        sceMotionStartSampling();
        g_motion_started = 1;
    }
#endif
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

#if SENSOR_USE_MOTION
    SceMotionSensorState st;
    if (sceMotionGetSensorState(&st, 1) >= 0) {
        // TODO(hw): confirm signs on a real console. Android: +X right,
        // +Y up, +Z out of the screen (landscape device = natural orientation).
        events[0].acceleration.x = -st.accelerometer.x * ASENSOR_STANDARD_GRAVITY;
        events[0].acceleration.y = -st.accelerometer.y * ASENSOR_STANDARD_GRAVITY;
        events[0].acceleration.z = -st.accelerometer.z * ASENSOR_STANDARD_GRAVITY;
        return 1;
    }
#endif
    events[0].acceleration.x = 0.0f;
    events[0].acceleration.y = 0.0f;
    events[0].acceleration.z = ASENSOR_STANDARD_GRAVITY;
    return 1;
}
