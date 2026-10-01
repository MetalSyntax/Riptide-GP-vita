/**
 * @file  looper.h
 * @brief ALooper + pipe() replacement for the android_native_app_glue that is
 *        statically linked inside libBlue.so.
 *
 * The glue (see ANativeActivity_onCreate @ 0x110410 in libBlue.so) creates a
 * pipe(), registers its read end with ALooper_addFd(LOOPER_ID_MAIN) and the
 * UI thread writes one-byte APP_CMD_* values into it. AInputQueue and the
 * ASensorEventQueue are attached to the same looper. ALooper_pollAll() here
 * reports whichever of those sources has data, exactly like bionic would.
 */

#ifndef SOLOADER_LOOPER_H
#define SOLOADER_LOOPER_H

#include <stdint.h>
#include <sys/types.h>

#define ALOOPER_POLL_WAKE     (-1)
#define ALOOPER_POLL_CALLBACK (-2)
#define ALOOPER_POLL_TIMEOUT  (-3)
#define ALOOPER_POLL_ERROR    (-4)

#define ALOOPER_EVENT_INPUT   (1 << 0)

typedef struct ALooper ALooper;
typedef int (*ALooper_callbackFunc)(int fd, int events, void *data);

/** Kind of source registered on the looper. */
typedef enum {
    LOOPER_SRC_FD = 0,     ///< fake pipe read end (glue command channel)
    LOOPER_SRC_INPUT,      ///< AInputQueue
    LOOPER_SRC_SENSOR,     ///< ASensorEventQueue
} looper_src_type;

ALooper *ALooper_prepare(int opts);
ALooper *ALooper_forThread(void);
int ALooper_addFd(ALooper *looper, int fd, int ident, int events,
                  ALooper_callbackFunc callback, void *data);
int ALooper_removeFd(ALooper *looper, int fd);
int ALooper_pollAll(int timeoutMillis, int *outFd, int *outEvents,
                    void **outData);
int ALooper_pollOnce(int timeoutMillis, int *outFd, int *outEvents,
                     void **outData);
void ALooper_wake(ALooper *looper);

/** Registers a non-fd source (input queue / sensor queue) on the looper. */
int looper_attach_source(ALooper *looper, looper_src_type type, void *obj,
                         int ident, void *data);
void looper_detach_source(ALooper *looper, void *obj);
/** Wakes up any thread blocked in ALooper_pollAll(). */
void looper_signal(void);

/* Fake pipe: fds are >= FAKE_PIPE_FD_BASE so they never collide with newlib. */
#define FAKE_PIPE_FD_BASE 0x7000

int pipe_soloader(int fds[2]);
ssize_t read_soloader(int fd, void *buf, size_t count);
ssize_t write_soloader(int fd, const void *buf, size_t count);
/** @return 1 if fd was a fake pipe end (and got closed), 0 otherwise. */
int fake_pipe_close(int fd);

#endif // SOLOADER_LOOPER_H
