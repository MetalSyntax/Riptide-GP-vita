/**
 * @file  sensor.h
 * @brief ASensorManager / ASensorEventQueue replacement (accelerometer only).
 *
 * android_main @ 0x10f150 does:
 *   mgr   = ASensorManager_getInstance();
 *   queue = ASensorManager_createEventQueue(mgr, app->looper, LOOPER_ID_USER, NULL, NULL);
 *   accel = ASensorManager_getDefaultSensor(mgr, ASENSOR_TYPE_ACCELEROMETER);
 * and on ident == LOOPER_ID_USER drains ASensorEventQueue_getEvents(queue, &ev, 1)
 * into OnSensorEvent(). Samples come from sceMotion.
 */

#ifndef SOLOADER_SENSOR_H
#define SOLOADER_SENSOR_H

#include <stdint.h>
#include <sys/types.h>
#include "reimpl/looper.h"

#define ASENSOR_TYPE_ACCELEROMETER 1
#define ASENSOR_STANDARD_GRAVITY   9.80665f

typedef struct ASensorManager ASensorManager;
typedef struct ASensor ASensor;
typedef struct ASensorEventQueue ASensorEventQueue;

/** Bionic ASensorEvent layout (104 bytes, android/sensor.h). */
typedef struct {
    int32_t version;
    int32_t sensor;
    int32_t type;
    int32_t reserved0;
    int64_t timestamp;
    union {
        float data[16];
        struct { float x, y, z; int8_t status; uint8_t reserved[3]; } acceleration;
    };
    int32_t reserved1[4];
} ASensorEvent;

int sensor_queue_ready(void *q);

ASensorManager *ASensorManager_getInstance(void);
const ASensor *ASensorManager_getDefaultSensor(ASensorManager *manager, int type);
ASensorEventQueue *ASensorManager_createEventQueue(ASensorManager *manager,
                                                   ALooper *looper, int ident,
                                                   ALooper_callbackFunc callback,
                                                   void *data);
int ASensorManager_destroyEventQueue(ASensorManager *manager, ASensorEventQueue *queue);
int ASensorEventQueue_enableSensor(ASensorEventQueue *queue, const ASensor *sensor);
int ASensorEventQueue_disableSensor(ASensorEventQueue *queue, const ASensor *sensor);
int ASensorEventQueue_setEventRate(ASensorEventQueue *queue, const ASensor *sensor,
                                   int32_t usec);
ssize_t ASensorEventQueue_getEvents(ASensorEventQueue *queue, ASensorEvent *events,
                                    size_t count);

#endif // SOLOADER_SENSOR_H
