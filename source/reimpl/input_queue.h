/**
 * @file  input_queue.h
 * @brief AInputQueue / AInputEvent / AMotionEvent / AKeyEvent replacement.
 *
 * Only the accessors libBlue.so imports are implemented (see `nm -D -u`):
 * AInputEvent_getDeviceId/getSource/getType, AKeyEvent_getAction/getKeyCode,
 * AMotionEvent_getAction/getPointerCount/getPointerId/getX/getY and the five
 * AInputQueue_* functions used by the glue's process_input().
 */

#ifndef SOLOADER_INPUT_QUEUE_H
#define SOLOADER_INPUT_QUEUE_H

#include <stdint.h>
#include "reimpl/looper.h"

#define AINPUT_EVENT_TYPE_KEY    1
#define AINPUT_EVENT_TYPE_MOTION 2

#define AINPUT_SOURCE_KEYBOARD    0x00000101
#define AINPUT_SOURCE_GAMEPAD     0x00000401
#define AINPUT_SOURCE_TOUCHSCREEN 0x00001002

#define AKEY_EVENT_ACTION_DOWN 0
#define AKEY_EVENT_ACTION_UP   1

#define AMOTION_EVENT_ACTION_DOWN         0
#define AMOTION_EVENT_ACTION_UP           1
#define AMOTION_EVENT_ACTION_MOVE         2
#define AMOTION_EVENT_ACTION_CANCEL       3
#define AMOTION_EVENT_ACTION_POINTER_DOWN 5
#define AMOTION_EVENT_ACTION_POINTER_UP   6
#define AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT 8

#define AKEYCODE_BACK 4
#define AKEYCODE_MENU 82

#define INPUT_MAX_POINTERS 8

typedef struct AInputQueue AInputQueue;
typedef struct AInputEvent AInputEvent;

struct AInputEvent {
    int32_t type;
    int32_t source;
    int32_t device_id;
    int32_t action;
    int32_t keycode;
    int32_t pointer_count;
    int32_t pointer_id[INPUT_MAX_POINTERS];
    float x[INPUT_MAX_POINTERS];
    float y[INPUT_MAX_POINTERS];
};

/** Creates the (single) queue handed to onInputQueueCreated(). */
AInputQueue *input_queue_create(void);
/** Thread-safe: called from the port's input thread. */
void input_queue_push(AInputQueue *q, const AInputEvent *ev);
/** Looper readiness check. Must not take the looper lock. */
int input_queue_has_events(void *q);

void AInputQueue_attachLooper(AInputQueue *queue, ALooper *looper, int ident,
                              ALooper_callbackFunc callback, void *data);
void AInputQueue_detachLooper(AInputQueue *queue);
int32_t AInputQueue_hasEvents(AInputQueue *queue);
int32_t AInputQueue_getEvent(AInputQueue *queue, AInputEvent **outEvent);
int32_t AInputQueue_preDispatchEvent(AInputQueue *queue, AInputEvent *event);
void AInputQueue_finishEvent(AInputQueue *queue, AInputEvent *event, int handled);

int32_t AInputEvent_getType(const AInputEvent *event);
int32_t AInputEvent_getSource(const AInputEvent *event);
int32_t AInputEvent_getDeviceId(const AInputEvent *event);
int32_t AKeyEvent_getAction(const AInputEvent *event);
int32_t AKeyEvent_getKeyCode(const AInputEvent *event);
int32_t AMotionEvent_getAction(const AInputEvent *event);
uint32_t AMotionEvent_getPointerCount(const AInputEvent *event);
int32_t AMotionEvent_getPointerId(const AInputEvent *event, uint32_t idx);
float AMotionEvent_getX(const AInputEvent *event, uint32_t idx);
float AMotionEvent_getY(const AInputEvent *event, uint32_t idx);

#endif // SOLOADER_INPUT_QUEUE_H
