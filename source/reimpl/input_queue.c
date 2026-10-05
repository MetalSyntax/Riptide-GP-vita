/**
 * @file  input_queue.c
 * @brief AInputQueue replacement fed by the port's input thread.
 */

#include "reimpl/input_queue.h"
#include "utils/logger.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE_CAPACITY 64

struct AInputQueue {
    pthread_mutex_t lock;
    AInputEvent ring[QUEUE_CAPACITY];
    int head, count;
    AInputEvent current; // event handed out by getEvent(), valid until finish
    ALooper *looper;
};

AInputQueue *input_queue_create(void) {
    AInputQueue *q = calloc(1, sizeof(AInputQueue));
    pthread_mutex_init(&q->lock, NULL);
    return q;
}

void input_queue_push(AInputQueue *q, const AInputEvent *ev) {
    if (!q)
        return;
    pthread_mutex_lock(&q->lock);
    if (q->count == QUEUE_CAPACITY) {
        // Drop the oldest event rather than blocking the input thread. Never
        // drop DOWN/UP pairs silently in practice: 64 is way above one frame.
        q->head = (q->head + 1) % QUEUE_CAPACITY;
        q->count--;
        l_warn("AInputQueue overflow, dropped oldest event");
    }
    q->ring[(q->head + q->count) % QUEUE_CAPACITY] = *ev;
    q->count++;
    pthread_mutex_unlock(&q->lock);
    looper_signal();
}

int input_queue_has_events(void *queue) {
    AInputQueue *q = queue;
    if (!q)
        return 0;
    pthread_mutex_lock(&q->lock);
    int ret = q->count > 0;
    pthread_mutex_unlock(&q->lock);
    return ret;
}

void AInputQueue_attachLooper(AInputQueue *queue, ALooper *looper, int ident,
                              ALooper_callbackFunc callback, void *data) {
    l_debug("AInputQueue_attachLooper(%p, %p, %i, %p, %p)", queue, looper,
            ident, callback, data);
    if (!queue)
        return;
    queue->looper = looper;
    looper_attach_source(looper, LOOPER_SRC_INPUT, queue, ident, data);
}

void AInputQueue_detachLooper(AInputQueue *queue) {
    l_debug("AInputQueue_detachLooper(%p)", queue);
    if (!queue)
        return;
    looper_detach_source(queue->looper, queue);
    queue->looper = NULL;
}

int32_t AInputQueue_hasEvents(AInputQueue *queue) {
    return input_queue_has_events(queue) ? 1 : 0;
}

int32_t AInputQueue_getEvent(AInputQueue *queue, AInputEvent **outEvent) {
    if (!queue)
        return -1;
    pthread_mutex_lock(&queue->lock);
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    queue->current = queue->ring[queue->head];
    queue->head = (queue->head + 1) % QUEUE_CAPACITY;
    queue->count--;
    pthread_mutex_unlock(&queue->lock);
    *outEvent = &queue->current;
    return 0;
}

int32_t AInputQueue_preDispatchEvent(AInputQueue *queue, AInputEvent *event) {
    // 0 = not consumed by an IME, the app must handle it.
    return 0;
}

void AInputQueue_finishEvent(AInputQueue *queue, AInputEvent *event, int handled) {
}

int32_t AInputEvent_getType(const AInputEvent *event) {
    return event ? event->type : 0;
}

int32_t AInputEvent_getSource(const AInputEvent *event) {
    return event ? event->source : 0;
}

int32_t AInputEvent_getDeviceId(const AInputEvent *event) {
    return event ? event->device_id : 0;
}

int32_t AKeyEvent_getAction(const AInputEvent *event) {
    return event ? event->action : 0;
}

int32_t AKeyEvent_getKeyCode(const AInputEvent *event) {
    return event ? event->keycode : 0;
}

int32_t AMotionEvent_getAction(const AInputEvent *event) {
    return event ? event->action : 0;
}

uint32_t AMotionEvent_getPointerCount(const AInputEvent *event) {
    return event ? (uint32_t)event->pointer_count : 0;
}

int32_t AMotionEvent_getPointerId(const AInputEvent *event, uint32_t idx) {
    if (!event || idx >= INPUT_MAX_POINTERS)
        return 0;
    return event->pointer_id[idx];
}

float AMotionEvent_getX(const AInputEvent *event, uint32_t idx) {
    if (!event || idx >= INPUT_MAX_POINTERS)
        return 0.0f;
    return event->x[idx];
}

float AMotionEvent_getY(const AInputEvent *event, uint32_t idx) {
    if (!event || idx >= INPUT_MAX_POINTERS)
        return 0.0f;
    return event->y[idx];
}

// Looked up with dlsym() for gamepad axes. Our pad goes through the MOGA JNI
// path (controls.c), so only touch events reach here: AXIS_X/AXIS_Y (0/1)
// mirror getX/getY, every other axis is at rest.
float AMotionEvent_getAxisValue(const AInputEvent *event, int32_t axis, uint32_t idx) {
    if (axis == 0)
        return AMotionEvent_getX(event, idx);
    if (axis == 1)
        return AMotionEvent_getY(event, idx);
    return 0.0f;
}
