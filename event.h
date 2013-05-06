/* 
 * File:   event.h
 * Author: oc
 *
 * Created on August 4, 2010, 6:04 PM
 */

#ifndef _EVENT_H
#define	_EVENT_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <semaphore.h>
#include <stdlib.h>
#include <errno.h>
#include <dbgflags/debug_macros.h>
#include "debug.h"

#define MODULE_FLAG FLAG_RUNTIME

typedef sem_t event_t;

static inline int createEvent(event_t *event,unsigned int signaled) {
    int error = EXIT_SUCCESS;
    if (sem_init(event, 0, signaled) != 0) {
        error = errno;
        ERROR_MSG("sem_init error %d (%m)",error);
    }
    return error;
}

static inline int destroyEven(event_t *event) {
    int error = EXIT_SUCCESS;
    if (sem_destroy(event) != 0) {
        error = errno;
        ERROR_MSG("sem_destroy error %d (%m)",error);
    }
    return error;
}

static inline int setEvent(event_t *event) {
    int error = EXIT_SUCCESS;
    if (sem_post(event) != 0) {
        error = errno;
        ERROR_MSG("sem_post error %d (%m)",error);
    }
    return error;
}

static inline int waitForEvent(event_t *event) {
    int error = EXIT_SUCCESS;
    if (sem_wait(event) != 0) {
        error = errno;
        /* needed when a debugger is in use */
        while(EINTR == error){
           if (sem_wait(event) != 0) {
               error = errno;
           } else {
               error = EXIT_SUCCESS;
           }
        }
        if (error != EXIT_SUCCESS) {
            ERROR_MSG("sem_wait error %d (%m)",error);
        }
    }
    return error;
}

#undef MODULE_FLAG

#ifdef	__cplusplus
}
#endif

#endif	/* _EVENT_H */

