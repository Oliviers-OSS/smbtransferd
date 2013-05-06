#include "globals.h"
#include "threadsPool.h"

#define MODULE_FLAG FLAG_RUNTIME
/*
#undef DEBUG_VAR
#define DEBUG_VAR(x,f)
#undef DEBUG_MSG
#define DEBUG_MSG(fmt,...)
#undef DEBUG_POSMSG
#define DEBUG_POSMSG(fmt,...)
 */
#define INTERNAL_FLAG_STOP     (1<<0)
#define INTERNAL_FLAG_DETACH  (1<<1)

/* to be able to free all allocated resources on exit */
static PROTECTED_LIST_HEAD_INIT(threadPoolAllocatedList);
//static protectedList_t threadPoolAllocatedList;
//pthread_once_t threadPoolAllocatedListInitOnceControl = PTHREAD_ONCE_INIT;
//static LIST_HEAD(threadPoolAllocatedList);

static void threadPoolAllocatedListInit(void)
{
    listInit(&threadPoolAllocatedList);
}

static inline int threadPoolAllocatedListAdd(threadPool_t *newThreadPool)
{
    struct list_head *newItem = &newThreadPool->threadPoolList;
    return listAdd(&threadPoolAllocatedList, newItem);
}

static inline int threadPoolAllocatedListRemove(threadPool_t *threadPool)
{
    int error = ENOENT;
    if (listGetWriteAccess(&threadPoolAllocatedList) == 0) {
        struct list_head *threadPoolList = &getList(threadPoolAllocatedList);
        struct list_head *pos = NULL;
        struct list_head *q = NULL;
        list_for_each_safe(pos, q, threadPoolList)
        {
            threadPool_t *item = list_entry(pos, threadPool_t, threadPoolList);
            if (item == threadPool) {
                list_del(pos);
                error = EXIT_SUCCESS;
                DEBUG_MSG("threadPool_t 0x%X removed from the allocated list",threadPool);
                break;
            }
        }
        listReleaseWriteAccess(&threadPoolAllocatedList);
    }
    return error;
}

static inline int threadPoolAllocatedListClear(void)
{
    int error = listGetWriteAccess(&threadPoolAllocatedList);
    DEBUG_MSG("%s",__FUNCTION__);
    if (0 == error) {
        struct list_head *threadPoolList = &getList(threadPoolAllocatedList);
        struct list_head *pos = NULL;
        struct list_head *q = NULL;

        list_for_each_safe(pos, q, threadPoolList)
        {
            threadPool_t *item = list_entry(pos, threadPool_t, threadPoolList);
            list_del(pos);
            if (item) {
                threadpoolDestroy(item);
            }
        }
        error = listReleaseWriteAccess(&threadPoolAllocatedList);
    }
    return error;
}

/*static inline*/ unsigned int threadPoolAllocatedListSize()
{
    unsigned int i = 0;
    int error = listGetReadAccess(&threadPoolAllocatedList);
    if (0 == error ) {
        struct list_head *pos = NULL;
        struct list_head *threadPoolList = &getList(threadPoolAllocatedList);

        list_for_each(pos, threadPoolList)
        {
            //threadPool_t *currentItem = list_entry(pos,threadPool_t,threadPoolList);
            i++;
        }
        listReleaseReadAccess(&threadPoolAllocatedList);
    } else {
        i = (unsigned int) - 1;
    }
    DEBUG_VAR(i, "%d");
    return i;
}

static pid_t __gettid()
{
    long res;
    __asm__ volatile ("int $0x80" \
  : "=a" (res) \
  : "0" (__NR_gettid));
    do {
        if ((unsigned long) (res) >= (unsigned long) (-(128 + 1))) {
            errno = -(res);
            res = -1;
        }
        return(pid_t) (res);
    } while (0);
}
#define gettid __gettid

pthread_once_t threadPoolAtExitSetOnceControl = PTHREAD_ONCE_INIT;

static void threadPoolClearAll(void)
{
    threadPoolAllocatedListClear();
}

static void atExitSet(void)
{
    atexit(threadPoolClearAll);
}

static inline int threadDataSetUnvailable(threadData_t *data)
{
    int error = ENOENT;
    struct list_head *threadDataList = &getList(data->owner->threadDataList);
    struct list_head *pos = NULL;
    struct list_head *q = NULL;

    if (listGetWriteAccess(&data->owner->threadDataList) == 0) {
        list_for_each_safe(pos, q, threadDataList)
        {
            threadData_t *item = list_entry(pos, threadData_t, list);
            if (data == item) {
                list_del(pos);
                data->owner->size--;
                DEBUG_VAR(data->owner->size, "%u");
                break;
            }
        }
        listReleaseWriteAccess(&data->owner->threadDataList);
    }

    return error;
}

static inline int threadDataFree(threadData_t *data)
{
    int error = EXIT_SUCCESS;

    error = pthread_mutex_destroy(&data->mutex);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_destroy error (%d)", error);
    }

    error = pthread_cond_destroy(&data->conditional);
    if (error != 0) {
        ERROR_MSG("pthread_cond_destroy error (%d)", error);
    }

    threadDataSetUnvailable(data);

    return error;
}

static void* threadPoolRuntime(void *param)
{
    void *threadReturn = NULL;
    threadData_t *data = (threadData_t *) param;
    int error = EXIT_SUCCESS;

    /* wait for a job */
    DEBUG_POSMSG("waiting for the mutex of 0x%X", data);
    error = pthread_mutex_lock(&data->mutex);
    DEBUG_POSMSG("have the mutex of 0x%X", data);
    if (0 == error) {
        unsigned int stop = 0;
        do {
            DEBUG_POSMSG("waiting for a job...(mutex release)");
            error = pthread_cond_wait(&data->conditional, &data->mutex);
            DEBUG_POSMSG("wake up & have the mutex of 0x%X", data);
            if (0 == error) {
                if (!(data->owner->internalsFlags & INTERNAL_FLAG_STOP)) {
                    DEBUG_MSG("wake up for a job");
                    if (-1 == data->runStatus) {
                        data->runStatus = WORK_IN_PROGRESS;
                        DEBUG_MSG("running function...");
                        data->runStatus = (*data->function)(data->directoryName);
                        //INFO_VAR(data->runStatus,"%d");
                    } else {
                        DEBUG_MSG("wake up with invalid status (%d)", data->runStatus);
                    }
                } else {
                    DEBUG_MSG("stop order received");
                    stop = 1;
                }
            } else {
                ERROR_MSG("pthread_cond_wait error (%d)", error);
                stop = 1;
            }

            if (sem_post(&data->owner->pool) != 0) {
                error = errno;
                ERROR_MSG("sem_post error (%d)", error);
                stop = 1;
            } else {
                DEBUG_POSMSG("semaphore release (incremented)");
            }
        } while (!stop);
        DEBUG_MSG("exiting...\n");
        error = pthread_mutex_unlock(&data->mutex);
        if (error != 0) {
            ERROR_MSG("pthread_mutex_unlock error (%d)", error);
        }
    } else {
        ERROR_MSG("pthread_mutex_lock error (%d)", error);
        if (sem_post(&data->owner->pool) != 0) {
            error = errno;
            ERROR_MSG("sem_post error %d (%m)", error);
        } else {
            DEBUG_POSMSG("semaphore release (incremented)");
        }
    }

    threadDataFree(data);
    //threadDataSetUnvailable(data);
    DEBUG_MSG("thread %d ended", gettid());
    return threadReturn;
}

inline int threadDataInit(threadPool_t *threadPool, threadData_t *data, thread_routine_t function)
{
    int error = EXIT_SUCCESS;
    data->runStatus = EXIT_SUCCESS;
    error = pthread_cond_init(&data->conditional, NULL);
    if (EXIT_SUCCESS == error) {
        error = pthread_mutex_init(&data->mutex, NULL);
        if (EXIT_SUCCESS == error) {
            //struct list_head *threadDataList = &getList(threadPool->threadDataList);
            DEBUG_VAR(&data->mutex, "0x%X");
            data->directoryName[0] = '\0';
            data->function = function;
            data->owner = threadPool;
            //list_add(&(data->list),threadDataList);
            error = listAdd(&threadPool->threadDataList, &(data->list));
            if (0 == error) {
                error = pthread_create(&data->thread, NULL, threadPoolRuntime, data);
                if (0 == error) {
                    if (threadPool->internalsFlags & INTERNAL_FLAG_DETACH) {
                        error = pthread_detach(data->thread);
                        if (error != 0) {
                            ERROR_MSG("pthread_detach error (%d)", error);
                        }
                    }
                } else {
                    ERROR_MSG("pthread_create error (%d)", error);
                }
            } else {
                ERROR_MSG("listAdd threadDataList error (%d)", error);
            }
        } else {
            ERROR_MSG("pthread_mutex_init error (%d)", error);
        }
    } else {
        ERROR_MSG("pthread_cond_init error (%d)", error);
    }
    return error;
}

static inline int threadDataAdd(threadPool_t *threadpool, thread_routine_t function)
{
    int error = EXIT_SUCCESS;
    threadData_t *newItem = (threadData_t *) malloc(sizeof(threadData_t));
    if (newItem) {
        const int error = threadDataInit(threadpool, newItem, function);
        if (error != EXIT_SUCCESS) {
            /* error already logged */
            list_del(&(newItem->list));
            free(newItem);
            newItem = NULL;
        }
    } else {
        error = ENOMEM;
        ERROR_MSG("failed to allocated %u bytes for threadData", sizeof(threadData_t));
    }
    return error;
}

unsigned int threadpoolCreate(threadPool_t *threadPool, const unsigned int size, thread_routine_t function)
{
    int error = EXIT_SUCCESS;
    int i = 0;

    //pthread_once(&threadPoolAllocatedListInitOnceControl, threadPoolAllocatedListInit);
    threadPool->size = 0;
    threadPool->internalsFlags = 0;
    error = listInit(&threadPool->threadDataList);
    if (EXIT_SUCCESS == error) {
        for (i = 0; i < size; i++) {
            error = threadDataAdd(threadPool, function);
            if (EXIT_SUCCESS == error) {
                threadPool->size++;
            } /* error already logged */
        }
        if (threadPool->size != 0) {
            if (sem_init(&threadPool->pool, 0, threadPool->size) == 0) {
                threadPoolAllocatedListAdd(threadPool);
                /*list_add(&getList(threadPool->threadPoolList),&threadPoolAllocatedList);*/
                pthread_once(&threadPoolAtExitSetOnceControl, atExitSet);                
                usleep(1000); /* BUG ? workaround to let the threads start and wait (else the cond_signal may be missed */
            } else {
                struct list_head *pos = NULL;
                struct list_head *q = NULL;

                error = errno;
                CRIT_MSG("sem_init pool of %d element error %d (%m)", threadPool->size, error);
                listGetWriteAccess(&threadPool->threadDataList);

                list_for_each_safe(pos, q, &getList(threadPool->threadDataList))
                {
                    threadData_t *item = list_entry(pos, threadData_t, list);
                    list_del(pos);
                    free(item);
                }
                listReleaseWriteAccess(&threadPool->threadDataList);
                threadPool->size = 0;
            } /* !sem_init */
        } /* (threadPool->size != 0) */
    } else {
        ERROR_MSG("listInitLock threadDataList error (%d)", error);
    }

    return error;
}

int threadpoolDestroy(threadPool_t *threadPool)
{
    int error = EXIT_SUCCESS;
    struct list_head *threadDataList = &getList(threadPool->threadDataList);

    DEBUG_MSG("%s",__FUNCTION__);
    threadPoolAllocatedListSize();
    /*
    * WARNING: don't lock the threadDataList to let the thread remove them from it
    */

    /* send the stop signal to all threads (must be in the cond_wait in the runtime) */
    threadPool->internalsFlags |= INTERNAL_FLAG_STOP;
    DEBUG_MARK;
    while (!list_empty(threadDataList)) {
        struct list_head *pos = NULL;
        struct list_head *q = NULL;

        DEBUG_MSG("sending stop signal");
        list_for_each_safe(pos, q, threadDataList)
        {
            threadData_t *data = list_entry(pos, threadData_t, list);
            if (data) {
                DEBUG_MSG("sending to 0x%X",data);
                error = pthread_cond_signal(&data->conditional);
                if (error != 0) {
                    ERROR_MSG("pthread_cond_signal error (%d)", error);
                }
                DEBUG_MSG("send to 0x%X",data);
            }
        }
        if (!list_empty(threadDataList)) {
            /* to give more time for the remaining threads */
            DEBUG_MSG("waiting before send stop signal again");
            usleep(1000);
            DEBUG_MARK;
        }
    } /* while (!list_empty(threadDataList))*/
DEBUG_MARK;
    if (!(threadPool->internalsFlags & INTERNAL_FLAG_DETACH)) {
        struct list_head *pos = NULL;        
        DEBUG_MSG("getting threads'return value");

        list_for_each(pos, threadDataList)
        {
            threadData_t *data = list_entry(pos, threadData_t, list);
            if (data) {
                void *threadReturn = NULL;
                error = pthread_join(data->thread, &threadReturn);
                if (error != 0) {
                    ERROR_MSG("pthread_join error (%d)", error);
                }
            }
        }
    }
DEBUG_MARK;
    if (sem_destroy(&threadPool->pool) != 0) {
        error = errno;
        ERROR_MSG("sem_destroy error %d (%m)", error);
    }
DEBUG_MARK;
    error = listFreeLock(&threadPool->threadDataList);
    if (error != 0) {
        ERROR_MSG("listFreeLock threadDataList error (%d)", error);
    }
DEBUG_MARK;
    error = threadPoolAllocatedListRemove(threadPool);
    if (error != EXIT_SUCCESS) {
        ERROR_MSG("threadPoolAllocatedListRemove error (%d)", error);
    }
DEBUG_MARK;
    threadPoolAllocatedListSize();
    return error;
}

threadData_t *threadpoolGetNextAvailable(threadPool_t *threadPool, threadPoolEntry_t *nextStart)
{
    int error = EXIT_SUCCESS;
    threadData_t *item = NULL;
    /* wait for an available worker thread */
    DEBUG_POSMSG("waiting for the semaphore");
    if (sem_wait(&threadPool->pool) == 0) {
        DEBUG_POSMSG("semaphore wake up, looking for an available thread");
        error = listGetReadAccess(&threadPool->threadDataList);
        if (0 == error) {
            struct list_head *pos = NULL;
            struct list_head *threadDataList = *nextStart;
            unsigned int i = 0;

            for (; (i < 2) && (NULL == item); i++) {

                list_for_each(pos, threadDataList)
                {
                    threadData_t *currentItem = list_entry(pos, threadData_t, list);
                    error = pthread_mutex_trylock(&currentItem->mutex);
                    if (0 == error) {
                        const unsigned int status = currentItem->runStatus;
                        DEBUG_POSMSG("mutex acquired");
                        error = pthread_mutex_unlock(&currentItem->mutex);
                        if (error != 0) {
                            ERROR_MSG("pthread_mutex_unlock error (%d)", error);
                            errno = error;
                        }
                        DEBUG_POSMSG("mutex released");
                        DEBUG_VAR(status, "%d");
                        if ((status != DATA_READY) && (status != WORK_IN_PROGRESS)) {
                            item = currentItem;
                            break;
                        }
                    } else if (error != EBUSY) {
                        ERROR_MSG("pthread_mutex_trylock error (%d)", error);
                        errno = error;
                    } else {
                        DEBUG_MSG("error == EBUSY");
                    }
                } /* list_for_each(pos,&threadData_list)*/
                DEBUG_VAR(i, "%d");
                if (NULL == item) {
                    usleep(125000);
                }
            } /* loop to manage the bad luck: scheduler wake up just after the sem_post AND before the pthread_cond_wait: try again once if there was only one available */
            listReleaseReadAccess(&threadPool->threadDataList);
        } /* 0 == listGetReadAccess */

        if (NULL == item) {
#ifdef _DEBUG_
            int sem_val = 0;
            if (sem_getvalue(&threadPool->pool, &sem_val) == 0) {
                DEBUG_VAR(sem_val, "%d");
            }
#endif
            ERROR_MSG("BUG: semaphore acquired but no worker thread available found");
            DEBUG_ONLY(abort());
            if (sem_post(&threadPool->pool) == 0) {
                errno = EAGAIN;
            } else {
                error = errno;
                ERROR_MSG("sem_post error %d (%m)", error);
            }
        }
    } else {
        error = errno;
        ERROR_MSG("sem_wait error %d (%m)", error);
        if (EINTR == error) {
            return threadpoolGetAvailable(threadPool);
        }
    }

    return item;
}
