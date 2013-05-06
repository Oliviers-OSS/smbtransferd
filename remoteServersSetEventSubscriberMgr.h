/* 
 * File:   remoteServersSetEventSubscriberMgr.h
 * Author: oc
 *
 * Created on November 21, 2010, 5:48 PM
 */

#ifndef _REMOTESERVERSSETEVENTSUBSCRIBERMGR_H_
#define	_REMOTESERVERSSETEVENTSUBSCRIBERMGR_H_

#include "globals.h"

#ifdef __cplusplus

#include <list>
#include <pthread.h>
#include <remoteServersSetC.h>
#include "messagesQueue.h"

#define MODULE_FLAG FLAG_CORBA

class remoteServersSetEventSubscriberMgr {
    struct subscriber{
        ::RemoteServersSet::EventListener_var listener;
        int lastError;
        unsigned long filter;

        subscriber(::RemoteServersSet::EventListener_ptr l,unsigned long f)
            :listener(l)
            ,lastError(EXIT_SUCCESS)
            ,filter(f) {
        }

        ~subscriber(){
        }
    };
    typedef std::list<subscriber*> eventsSubscribers;

    eventsSubscribers subscribers;    
    
    #undef MODULE_FLAG
    #define MODULE_FLAG  FLAG_DEADLOCK
    class lock {
        public:
            lock() {
            }
            ~lock() {
            }
            virtual int getRead() = 0;
            virtual int releaseRead() = 0;
            virtual int getWrite() = 0;
            virtual int releaseWrite() = 0;
    };

    #ifdef _GNU_SOURCE

    class rwLock : public lock{
        pthread_rwlock_t rwlock;
    public:
        rwLock() {
            pthread_rwlock_init(&rwlock,NULL);
        }
        ~rwLock() {
            pthread_rwlock_destroy(&rwlock);
        }
        virtual int getRead() {
             const int error(pthread_rwlock_rdlock(&rwlock));
             DEBUG_VAR(error,"%d");
             return error;
        }
        virtual int releaseRead() {
            const int error(pthread_rwlock_unlock(&rwlock));
            DEBUG_VAR(error,"%d");
            return error;
        }
        virtual int getWrite() {
            const int error(pthread_rwlock_wrlock(&rwlock));
            DEBUG_VAR(error,"%d");
            return error;
        }
        virtual int releaseWrite(){
            const int error(pthread_rwlock_unlock(&rwlock));
            DEBUG_VAR(error,"%d");
            return error;
        }
    } subscribersLock;

    #else /* _GNU_SOURCE */

    class mutex : public lock {
        pthread_mutex_t mutex_;
    public:
        mutex() {
            pthread_mutex_init(&mutex_,NULL);
        }
        ~mutex() {
            pthread_mutex_destroy(&mutex_);
        }
        virtual int getRead() {
            const int error(pthread_mutex_lock(&mutex_));
            DEBUG_VAR(error,"%d");
            return error;
        }
        virtual int releaseRead() {
            const int error(pthread_mutex_unlock(&mutex_));
            DEBUG_VAR(error,"%d");
            return error;
        }
        virtual int getWrite() {
            const int error(pthread_mutex_lock(&mutex_));
            DEBUG_VAR(error,"%d");
            return error;
        }
        virtual int releaseWrite(){
            const int error(pthread_mutex_unlock(&mutex_));
            DEBUG_VAR(error,"%d");
            return error;
        }
    } subscribersLock;

#endif /* _GNU_SOURCE */

    class readLockMgr {
        lock *managedLock;
    public:
        readLockMgr(lock *l)
           :managedLock(l) {
              managedLock->getRead();
        }
        readLockMgr(lock &l)
           :managedLock(&l) {
              managedLock->getRead();
        }
        ~readLockMgr() {
            managedLock->releaseRead();
        }
    };

    class writeLockMgr {
        lock *managedLock;
    public:
        writeLockMgr(lock *l)
           :managedLock(l) {
              managedLock->getWrite();
        }
        writeLockMgr(lock &l)
           :managedLock(&l) {
              managedLock->getWrite();
        }
        ~writeLockMgr() {
            managedLock->releaseWrite();
        }
    };

    #undef MODULE_FLAG
    #define MODULE_FLAG FLAG_CORBA
    remoteServersSetEventSubscriberMgr(const remoteServersSetEventSubscriberMgr& orig);
    inline eventsSubscribers::iterator find(::RemoteServersSet::EventListener_ptr listener);
    inline void clear();

    pthread_t eventsNotifierTID;
    messageQueueId eventsNotifierMQ;
    
public:
    remoteServersSetEventSubscriberMgr();    
    virtual ~remoteServersSetEventSubscriberMgr();

    int add(::RemoteServersSet::EventListener_ptr newListener,const unsigned long filter);
    int del(::RemoteServersSet::EventListener_ptr oldListener);    

    struct eventMsg {
      int level;
      long status;
      char message[PATH_MAX + 256];
     };
     inline int publish(const eventMsg &event);
     
private:
     inline void notify(const eventMsg &event);
     friend void* eventsNotifier(void *param);

};

extern remoteServersSetEventSubscriberMgr theRemoteServersSetEventSubscriberMgr;

#undef MODULE_FLAG

#endif /* __cplusplus */

#ifdef __cplusplus
extern "C"
{
#endif
    
    extern const unsigned long eventError;
    extern const unsigned long eventWarning;
    extern const unsigned long eventInformation;

#ifdef CORBA_INTERFACE
    int publishEvent(const unsigned long level,const int status,const  char *message, ...);
#else
    #define publishEvent(level,status,MessagesMgr, ...)
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _REMOTESERVERSSETEVENTSUBSCRIBERMGR_H_ */

