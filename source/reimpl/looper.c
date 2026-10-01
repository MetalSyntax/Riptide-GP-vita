/**
 * @file  looper.c
 * @brief ALooper + fake pipe() for the statically linked native_app_glue.
 *
 * Only one looper exists (the one prepared by the glue thread that runs
 * android_main), so ALooper_prepare()/ALooper_forThread() return a global.
 *
 * Readiness rules (android_main keeps calling ALooper_pollAll() until it
 * returns < 0 and only then runs OnStep(), see android_main @ 0x10f150), so
 * every source must stop being "ready" once the glue consumed it:
 *   - fake pipe: bytes pending in its ring buffer (consumed by read()).
 *   - input queue: events pending (consumed by AInputQueue_getEvent()).
 *   - sensor queue: a sample is due (consumed by ASensorEventQueue_getEvents()).
 */

#include "reimpl/looper.h"
#include "reimpl/input_queue.h"
#include "reimpl/sensor.h"
#include "utils/logger.h"

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

/* ---------------------------------------------------------------------------
 * Global wake-up primitive shared by all sources
 * ------------------------------------------------------------------------ */

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cond = PTHREAD_COND_INITIALIZER;
static volatile int    g_wake = 0;

void looper_signal(void) {
    pthread_mutex_lock(&g_lock);
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_lock);
}

/* ---------------------------------------------------------------------------
 * Fake pipe
 * ------------------------------------------------------------------------ */

#define FAKE_PIPE_MAX  8
#define FAKE_PIPE_SIZE 512

typedef struct {
    int used;
    int read_open, write_open;
    uint8_t buf[FAKE_PIPE_SIZE];
    int head, count;
} fake_pipe;

static fake_pipe g_pipes[FAKE_PIPE_MAX];

static fake_pipe *pipe_from_fd(int fd, int *is_write) {
    if (fd < FAKE_PIPE_FD_BASE || fd >= FAKE_PIPE_FD_BASE + FAKE_PIPE_MAX * 2)
        return NULL;
    int idx = (fd - FAKE_PIPE_FD_BASE) / 2;
    if (!g_pipes[idx].used)
        return NULL;
    if (is_write)
        *is_write = (fd - FAKE_PIPE_FD_BASE) & 1;
    return &g_pipes[idx];
}

int pipe_soloader(int fds[2]) {
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < FAKE_PIPE_MAX; i++) {
        if (!g_pipes[i].used) {
            memset(&g_pipes[i], 0, sizeof(fake_pipe));
            g_pipes[i].used = 1;
            g_pipes[i].read_open = g_pipes[i].write_open = 1;
            fds[0] = FAKE_PIPE_FD_BASE + i * 2;
            fds[1] = FAKE_PIPE_FD_BASE + i * 2 + 1;
            pthread_mutex_unlock(&g_lock);
            l_debug("pipe(): fake pipe %i -> [%i, %i]", i, fds[0], fds[1]);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_lock);
    l_error("pipe(): out of fake pipes");
    return -1;
}

ssize_t read_soloader(int fd, void *buf, size_t count) {
    int is_write;
    fake_pipe *p = pipe_from_fd(fd, &is_write);
    if (!p)
        return read(fd, buf, count);

    pthread_mutex_lock(&g_lock);
    // Blocking read, like a real pipe: wait until a writer puts something in.
    while (p->count == 0 && p->write_open)
        pthread_cond_wait(&g_cond, &g_lock);

    size_t n = 0;
    while (n < count && p->count > 0) {
        ((uint8_t *)buf)[n++] = p->buf[p->head];
        p->head = (p->head + 1) % FAKE_PIPE_SIZE;
        p->count--;
    }
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_lock);
    return (ssize_t)n;
}

ssize_t write_soloader(int fd, const void *buf, size_t count) {
    int is_write;
    fake_pipe *p = pipe_from_fd(fd, &is_write);
    if (!p)
        return write(fd, buf, count);

    pthread_mutex_lock(&g_lock);
    size_t n = 0;
    while (n < count) {
        while (p->count == FAKE_PIPE_SIZE && p->read_open)
            pthread_cond_wait(&g_cond, &g_lock);
        if (!p->read_open)
            break;
        int tail = (p->head + p->count) % FAKE_PIPE_SIZE;
        p->buf[tail] = ((const uint8_t *)buf)[n++];
        p->count++;
    }
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_lock);
    return n ? (ssize_t)n : -1;
}

int fake_pipe_close(int fd) {
    int is_write;
    fake_pipe *p = pipe_from_fd(fd, &is_write);
    if (!p)
        return 0;

    pthread_mutex_lock(&g_lock);
    if (is_write)
        p->write_open = 0;
    else
        p->read_open = 0;
    if (!p->read_open && !p->write_open)
        p->used = 0;
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_lock);
    return 1;
}

/* ---------------------------------------------------------------------------
 * ALooper
 * ------------------------------------------------------------------------ */

#define LOOPER_MAX_SOURCES 8

typedef struct {
    int used;
    looper_src_type type;
    int fd;
    void *obj;
    int ident;
    void *data;
} looper_source;

struct ALooper {
    looper_source src[LOOPER_MAX_SOURCES];
};

static ALooper g_looper;

ALooper *ALooper_prepare(int opts) {
    l_debug("ALooper_prepare(%i)", opts);
    return &g_looper;
}

ALooper *ALooper_forThread(void) {
    return &g_looper;
}

static int add_source(ALooper *looper, looper_src_type type, int fd, void *obj,
                      int ident, void *data) {
    if (!looper)
        looper = &g_looper;

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < LOOPER_MAX_SOURCES; i++) {
        if (!looper->src[i].used) {
            looper->src[i] = (looper_source){ 1, type, fd, obj, ident, data };
            pthread_cond_broadcast(&g_cond);
            pthread_mutex_unlock(&g_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_lock);
    l_error("ALooper: out of source slots");
    return -1;
}

int ALooper_addFd(ALooper *looper, int fd, int ident, int events,
                  ALooper_callbackFunc callback, void *data) {
    l_debug("ALooper_addFd(%p, fd %i, ident %i, events %i, cb %p, data %p)",
            looper, fd, ident, events, callback, data);
    if (callback)
        l_warn("ALooper_addFd: callbacks are not supported, ignoring");
    if (!pipe_from_fd(fd, NULL))
        l_warn("ALooper_addFd: fd %i is not a fake pipe, it will never fire", fd);
    return add_source(looper, LOOPER_SRC_FD, fd, NULL, ident, data);
}

int ALooper_removeFd(ALooper *looper, int fd) {
    if (!looper)
        looper = &g_looper;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < LOOPER_MAX_SOURCES; i++) {
        if (looper->src[i].used && looper->src[i].type == LOOPER_SRC_FD &&
            looper->src[i].fd == fd) {
            looper->src[i].used = 0;
            pthread_mutex_unlock(&g_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int looper_attach_source(ALooper *looper, looper_src_type type, void *obj,
                         int ident, void *data) {
    l_debug("looper_attach_source(type %i, obj %p, ident %i, data %p)",
            type, obj, ident, data);
    return add_source(looper, type, -1, obj, ident, data);
}

void looper_detach_source(ALooper *looper, void *obj) {
    if (!looper)
        looper = &g_looper;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < LOOPER_MAX_SOURCES; i++) {
        if (looper->src[i].used && looper->src[i].obj == obj)
            looper->src[i].used = 0;
    }
    pthread_mutex_unlock(&g_lock);
}

void ALooper_wake(ALooper *looper) {
    pthread_mutex_lock(&g_lock);
    g_wake = 1;
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_lock);
}

// Must be called with g_lock held.
static looper_source *find_ready(void) {
    for (int i = 0; i < LOOPER_MAX_SOURCES; i++) {
        looper_source *s = &g_looper.src[i];
        if (!s->used)
            continue;
        switch (s->type) {
            case LOOPER_SRC_FD: {
                fake_pipe *p = pipe_from_fd(s->fd, NULL);
                if (p && p->count > 0)
                    return s;
                break;
            }
            case LOOPER_SRC_INPUT:
                if (input_queue_has_events(s->obj))
                    return s;
                break;
            case LOOPER_SRC_SENSOR:
                if (sensor_queue_ready(s->obj))
                    return s;
                break;
        }
    }
    return NULL;
}

int ALooper_pollAll(int timeoutMillis, int *outFd, int *outEvents,
                    void **outData) {
    struct timespec deadline;
    if (timeoutMillis > 0) {
        struct timeval now;
        gettimeofday(&now, NULL);
        uint64_t ns = (uint64_t)now.tv_usec * 1000ULL +
                      (uint64_t)timeoutMillis * 1000000ULL;
        deadline.tv_sec = now.tv_sec + (time_t)(ns / 1000000000ULL);
        deadline.tv_nsec = (long)(ns % 1000000000ULL);
    }

    pthread_mutex_lock(&g_lock);
    for (;;) {
        looper_source *s = find_ready();
        if (s) {
            int ident = s->ident;
            if (outFd) *outFd = s->fd;
            if (outEvents) *outEvents = ALOOPER_EVENT_INPUT;
            if (outData) *outData = s->data;
            pthread_mutex_unlock(&g_lock);
            return ident;
        }

        if (g_wake) {
            g_wake = 0;
            pthread_mutex_unlock(&g_lock);
            return ALOOPER_POLL_WAKE;
        }

        if (timeoutMillis == 0)
            break;

        if (timeoutMillis < 0) {
            pthread_cond_wait(&g_cond, &g_lock);
        } else {
            // Sensor readiness is time based (no one signals it), so never
            // sleep longer than one sensor period in a single wait.
            struct timeval now;
            gettimeofday(&now, NULL);
            uint64_t now_ns = (uint64_t)now.tv_sec * 1000000000ULL + now.tv_usec * 1000ULL;
            uint64_t dl_ns = (uint64_t)deadline.tv_sec * 1000000000ULL + deadline.tv_nsec;
            if (now_ns >= dl_ns)
                break;
            uint64_t step_ns = now_ns + 16ULL * 1000000ULL;
            if (step_ns > dl_ns)
                step_ns = dl_ns;
            struct timespec step = { (time_t)(step_ns / 1000000000ULL),
                                     (long)(step_ns % 1000000000ULL) };
            pthread_cond_timedwait(&g_cond, &g_lock, &step);
        }
    }
    pthread_mutex_unlock(&g_lock);
    return ALOOPER_POLL_TIMEOUT;
}

int ALooper_pollOnce(int timeoutMillis, int *outFd, int *outEvents,
                     void **outData) {
    return ALooper_pollAll(timeoutMillis, outFd, outEvents, outData);
}
