/* 
 * File:   list.h
 * Author: oc
 *
 * Created on August 2, 2010, 12:05 PM
 */

#ifndef _LIST_H
#define	_LIST_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "klist.h"
#include <pthread.h>
#include <stdlib.h>
#include <dbgflags/debug_macros.h>
#include "debug.h"
#define MODULE_FLAG FLAG_RUNTIME

typedef struct protectedList_ {
    struct list_head list;
#ifdef _GNU_SOURCE
    pthread_rwlock_t rwlock;
#else
    pthread_mutex_t mutex;
#endif
} protectedList_t;

#ifdef _GNU_SOURCE

#define PROTECTED_LIST_HEAD_INIT(name)  protectedList_t name = { \
    { &(name.list), &(name.list) } \
    ,PTHREAD_RWLOCK_INITIALIZER \
}

static inline int listInit(protectedList_t *plist) {
    int error = EXIT_SUCCESS;
    error = pthread_rwlock_init(&plist->rwlock,NULL);
    if (error != 0) {
        ERROR_MSG("pthread_rwlock_init error (%d)",error);
    }
    INIT_LIST_HEAD(&plist->list);
    return error;
}

static inline int listGetReadAccess(protectedList_t *plist) {
    int error = pthread_rwlock_rdlock(&plist->rwlock);
    if (error != 0) {
        ERROR_MSG("pthread_rwlock_rdlock error (%d)",error);
    }
    return error;
}

static inline int listReleaseReadAccess(protectedList_t *plist) {
    int error = pthread_rwlock_unlock(&plist->rwlock);
    if (error != 0) {
        ERROR_MSG("pthread_rwlock_unlock error (%d)",error);
    }
    return error;
}

static inline int listGetWriteAccess(protectedList_t *plist) {
    int error = pthread_rwlock_wrlock(&plist->rwlock);
    if (error != 0) {
        ERROR_MSG("pthread_rwlock_wrlock error (%d)",error);
    }
    return error;
}

static inline int listReleaseWriteAccess(protectedList_t *plist) {
    int error = pthread_rwlock_unlock(&plist->rwlock);
    if (error != 0) {
        ERROR_MSG("pthread_rwlock_unlock error (%d)",error);
    }
    return error;
}

static inline int listFreeLock(protectedList_t *plist) {
    int error = pthread_rwlock_destroy(&plist->rwlock);
    if (error != 0) {
        ERROR_MSG("pthread_rwlock_destroy error (%d)",error);
    }
    return error;
}

#else /* _GNU_SOURCE */

#define PROTECTED_LIST_HEAD_INIT(name)  protectedList_t name = { \
    { &(threadPoolAllocatedList.list), &(threadPoolAllocatedList.list) } \
    ,PTHREAD_MUTEX_INITIALIZER \
}

static inline int listInit(protectedList_t *plist) {
    int error = EXIT_SUCCESS;
    error = pthread_mutex_init(&plist->mutex, NULL);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_init error (%d)",error);
    }
    INIT_LIST_HEAD(&plist->list);
    return error;
}

static inline int listGetReadAccess(protectedList_t *plist) {
    int error = pthread_mutex_lock(&plist->mutex);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_lock error (%d)",error);
    }
    return error;
}

static inline int listReleaseReadAccess(protectedList_t *plist) {
    int error = pthread_mutex_unlock(&plist->mutex);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_unlock error (%d)",error);
    }
    return error;
}

static inline int listGetWriteAccess(protectedList_t *plist) {
    int error = pthread_mutex_lock(&plist->mutex);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_lock error (%d)",error);
    }
    return error;
}

static inline int listReleaseWriteAccess(protectedList_t *plist) {
    int error = pthread_mutex_unlock(&plist->mutex);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_unlock error (%d)",error);
    }
    return error;
}

static inline int listFreeLock(protectedList_t *plist) {
    int error = pthread_mutex_destroy(&plist->mutex);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_destroy error (%d)",error);
    }
    return error;
}

#endif /* _GNU_SOURCE */

#define getList(x)  (x.list)

static inline int listAdd(protectedList_t *plist,struct list_head *newItem) {
    int error = listGetWriteAccess(plist);
    if (0 == error) {
        list_add(newItem,&plist->list);
        error = listReleaseWriteAccess(plist); /* error already logged */
    } /* error already logged */
    return error;
}
#undef MODULE_FLAG

#ifdef	__cplusplus
}
#endif

#endif	/* _LIST_H */

