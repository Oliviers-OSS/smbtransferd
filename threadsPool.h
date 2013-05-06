#ifndef _THREADSPOOL_H_
#define	_THREADSPOOL_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <semaphore.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "list.h"
#include "event.h"

#ifdef _DEBUG_
#define INFO_VAR(x,f)        FILTER LOGGER(LOG_INFO|LOG_OPTS, SIMPLE_DEBUG_LOG_HEADER __FILE__ "(%d) %s: " #x " = " f DEBUG_EOL,__LINE__,__FUNCTION__,x)
#define DEBUG_ONLY(x)        x
#else
    #define INFO_VAR(x,f)
    #define DEBUG_ONLY(x)
#endif

#define DATA_READY                -1
#define WORK_IN_PROGRESS        EBUSY

#define MODULE_FLAG FLAG_RUNTIME

typedef int (*thread_routine_t)(const char*);

struct threadPoolData_;
typedef struct threadData_ {    
    struct list_head list;
    pthread_mutex_t mutex;
    event_t wakeUp;
    pthread_t thread;
    unsigned int runStatus;            
    char directoryName[PATH_MAX];
    thread_routine_t function;
    struct threadPoolData_ *owner;    
} threadData_t;

typedef struct threadPoolData_ {    
    sem_t pool;
    unsigned int size;
    unsigned int internalsFlags;
    protectedList_t threadDataList;
    struct list_head threadPoolList;
} threadPool_t;

static inline int threadDataSetWork(threadData_t *data,const char *directoryName) {
    int error = EXIT_SUCCESS;

    DEBUG_POSMSG("waiting for mutex of 0x%X",data);
    error = pthread_mutex_lock(&data->mutex);    
    if (0 == error ) {
        DEBUG_POSMSG("have mutex of 0x%X",data);
        strcpy(data->directoryName,directoryName);
        data->runStatus = DATA_READY; /* reset status code */
        DEBUG_POSMSG("releasing mutex of 0x%X",data);
        error = pthread_mutex_unlock(&data->mutex);
        DEBUG_POSMSG("mutex released of 0x%X",data);
        if (0 == error ) {
            DEBUG_POSMSG("signaling to 0x%X...",data);
            error = setEvent(&data->wakeUp); /* error already logged */
            DEBUG_POSMSG("0x%X signaled !",data);            
        } else {
            ERROR_MSG("pthread_mutex_unlock error (%d)",error);
        }
    } else {
        ERROR_MSG("pthread_mutex_lock error (%d)",error);
    }
        
    return error;
}

static inline int threadDataGetDirBuffer(threadData_t *data,const char **directoryName) {
    int error = EXIT_SUCCESS;
    DEBUG_POSMSG("waiting for mutex of 0x%X",data);
    error = pthread_mutex_lock(&data->mutex);
    if (0 == error) {
        *directoryName = data->directoryName;
    } else {
        ERROR_MSG("pthread_mutex_lock error (%d)",error);
    }
    return error;
}

static inline int threadDataReleaseDirBuffer(threadData_t *data) {
    int error = EXIT_SUCCESS;
    data->runStatus = DATA_READY; /* reset status code */
    DEBUG_POSMSG("releasing mutex of 0x%X",data);
    error = pthread_mutex_unlock(&data->mutex);
    DEBUG_POSMSG("mutex released of 0x%X",data);
    if (0 == error ) {
        DEBUG_POSMSG("signaling to 0x%X...",data);
        error = setEvent(&data->wakeUp); /* error already logged */
        DEBUG_POSMSG("0x%X signaled !",data);
    } else {
        ERROR_MSG("pthread_mutex_unlock error (%d)",error);
    }
    return error;
}

unsigned int threadPoolCreate(threadPool_t *threadPool,const unsigned int size,thread_routine_t function);
int threadPoolDestroy(threadPool_t *threadPool);

//threadData_t *threadpoolGetAvailable(threadPool_t *threadPool);
typedef struct list_head *threadPoolEntry_t;

threadData_t *threadPoolGetNextAvailable(threadPool_t *threadPool,threadPoolEntry_t *nextStart);

static inline threadData_t *threadpoolGetAvailable(threadPool_t *threadPool) {
    threadPoolEntry_t start = &getList(threadPool->threadDataList);
    return threadPoolGetNextAvailable(threadPool,&start);
}

static inline threadData_t *threadpoolGetFirstAvailable(threadPool_t *threadPool,threadPoolEntry_t *nextStart) {
    *nextStart = &getList(threadPool->threadDataList);
    return threadPoolGetNextAvailable(threadPool,nextStart);
}

#undef MODULE_FLAG

#ifdef	__cplusplus
}
#endif

#endif	/* _THREADSPOOL_H_ */

