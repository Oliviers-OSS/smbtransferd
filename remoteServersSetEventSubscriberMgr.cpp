/* 
 * File:   remoteServersSetEventSubscriberMgr.cpp
 * Author: oc
 * 
 * Created on November 21, 2010, 5:48 PM
 */

#include <malloc.h>
#include "remoteServersSetEventSubscriberMgr.h"
#include <algorithm>
#include <assert.h>
#include <tao/Objref_VarOut_T.h>
#include <tao/Object.h>
#include <cstdio>
#include <cstdarg>
#include "remoteServersSetC.h"

#define QUEUE_NAME "/eventsMQ"
#define MODULE_FLAG FLAG_NOTIFICATION

remoteServersSetEventSubscriberMgr theRemoteServersSetEventSubscriberMgr;
const unsigned long eventError = RemoteServersSet::EventSubscriber::errorsEventFilters;
const unsigned long eventWarning = RemoteServersSet::EventSubscriber::warningsEventFilters;
const unsigned long eventInformation = RemoteServersSet::EventSubscriber::informationEventFilters;

inline void remoteServersSetEventSubscriberMgr::notify(const eventMsg &event) {
    writeLockMgr lock(subscribersLock);
    eventsSubscribers::iterator i = subscribers.begin();
    const eventsSubscribers::iterator end = subscribers.end();
    ::RemoteServersSet::Event newEvent;

    newEvent.status = event.status;
    newEvent.message = CORBA::string_dup(event.message);    
    DEBUG_MSG("notifying event (%s,%d) (level %d)",event.message,event.status,event.level);
    while (i != end) {
        try {
            subscriber &s = *(*i);
            try {
                DEBUG_MSG("listener 0x%X, mask level = %d",&s.listener,s.filter);
                if (event.level & s.filter) {                    
                    /*if  (!s.listener->_non_existent()) HANGS !*/ {
                        DEBUG_MSG("notifying to 0x%X...",&s.listener);
                        s.listener->notifyEvent(newEvent);
                        s.lastError = EXIT_SUCCESS;
                    }
                } else {
                    DEBUG_MSG("filtered (%d & %d)",event.level, s.filter);
                }
            }//try
            catch (CORBA::UserException &exception) {
                if (EXIT_SUCCESS == s.lastError) {
                    s.lastError = EIO;
                } else {
                    subscribers.erase(i);
                    delete (*i);
                }
                ERROR_STREAM << "CORBA::UserException " << exception << std::endl;
            }
            catch (CORBA::SystemException &exception) {
                if (EXIT_SUCCESS == s.lastError) {
                    s.lastError = EIO;
                } else {
                    subscribers.erase(i);
                    delete (*i);
                }
                ERROR_STREAM << "CORBA::SystemException " << exception << std::endl;
            }
            catch (CORBA::Exception &exception) {
                if (EXIT_SUCCESS == s.lastError) {
                    s.lastError = EIO;
                } else {
                    subscribers.erase(i);
                    delete (*i);
                }
                ERROR_STREAM << "CORBA::Exception " << exception << std::endl;                
            }
            catch(...) {
                s.lastError = EFAULT;
                ERROR_STREAM << "exception caught!" << std::endl;
            }
            i++;
        }//try
        catch (...) {
            ERROR_STREAM << "exception caught !" << std::endl;
        }
    } // while (i != end)
}

void* eventsNotifier(void *param) {
    messageQueueId eventsMQ;
    int error = createMessagesQueue(QUEUE_NAME, O_RDONLY, &eventsMQ);
    if (EXIT_SUCCESS == error) {
        do {
            /* wait for an event */
            char message[8192]; /* WARNING: this value can't be changed else the read will fail */
            size_t messageLength = sizeof (message);
            unsigned int messagePriority = 0;
            error = readFromMessagesQueue(eventsMQ, message, &messageLength, &messagePriority);
            if (EXIT_SUCCESS == error) {
                if (sizeof (remoteServersSetEventSubscriberMgr::eventMsg) == messageLength) {
                    const remoteServersSetEventSubscriberMgr::eventMsg *event = (remoteServersSetEventSubscriberMgr::eventMsg *)message;
                    DEBUG_MSG("new event read");
                    theRemoteServersSetEventSubscriberMgr.notify(*event);
                } else {
                    ERROR_MSG("invalid message received (lenght = %d)", messageLength);
                }
            } else {
                ERROR_MSG("readFromMessagesQueue " QUEUE_NAME " error %d",error);
            }
        } while (1);
        closeMessagesQueue(eventsMQ);
    } else {
        /* error already logged */
    }
    return NULL;
}

inline int remoteServersSetEventSubscriberMgr::publish(const eventMsg & event) {
    return writeToMessagesQueue(eventsNotifierMQ, (char*) & event, sizeof (eventMsg), 0);
}

remoteServersSetEventSubscriberMgr::remoteServersSetEventSubscriberMgr()
    : eventsNotifierTID(0)
    , eventsNotifierMQ(-1) {
    int error = createMessagesQueue(QUEUE_NAME, O_WRONLY, &eventsNotifierMQ);
    if (EXIT_SUCCESS == error) {
        error = pthread_create(&eventsNotifierTID, NULL, eventsNotifier, NULL);
        if (error != 0) {
            ERROR_MSG("pthread_create eventsNotifier error %d (%m)", error);
        }
    } else {
        /* error already logged */
    }
}

remoteServersSetEventSubscriberMgr::remoteServersSetEventSubscriberMgr(const remoteServersSetEventSubscriberMgr & orig) {
    // singleton: copy not allowed
}

inline void remoteServersSetEventSubscriberMgr::clear() {
    writeLockMgr lock(subscribersLock);
    eventsSubscribers::iterator i = subscribers.begin();
    const eventsSubscribers::iterator end = subscribers.end();
    while (i != end) {
        const remoteServersSetEventSubscriberMgr::subscriber *s = *i;
        subscribers.erase(i);
        delete s;
        i++;
    }
}

remoteServersSetEventSubscriberMgr::~remoteServersSetEventSubscriberMgr() {
    clear();
    if (eventsNotifierMQ != -1) {
        closeMessagesQueue(eventsNotifierMQ);
    }
}

inline remoteServersSetEventSubscriberMgr::eventsSubscribers::iterator remoteServersSetEventSubscriberMgr::find(::RemoteServersSet::EventListener_ptr listener) {
    readLockMgr lock(subscribersLock);
    eventsSubscribers::iterator item = subscribers.end();
    eventsSubscribers::iterator i = subscribers.begin();
    const eventsSubscribers::iterator end = item;
    while ((i != end) && (item == end)) {
        const remoteServersSetEventSubscriberMgr::subscriber *s = *i;
        assert(s);
        if (s->listener->_is_equivalent(listener)) {
            item = i;
        } else {
            i++;
        }
    }
    return item;
}

int remoteServersSetEventSubscriberMgr::add(::RemoteServersSet::EventListener_ptr newListener, const unsigned long filter) {
    int error(EXIT_SUCCESS);
    remoteServersSetEventSubscriberMgr::eventsSubscribers::iterator i(find(newListener));
    if (i == subscribers.end()) {
        subscriber *newSubscriber = new(std::nothrow) subscriber(::RemoteServersSet::EventListener::_duplicate(newListener), filter);
        if (newSubscriber) {
            writeLockMgr lock(subscribersLock);
            subscribers.push_back(newSubscriber);
        } else {
            ERROR_MSG("failed to allocate %d bytes for a new subscriber", sizeof (subscriber));
            error = ENOMEM;
        }
    } else {
        WARNING_MSG("subscriber already registered");
        error = EEXIST;
    }
    return error;
}

int remoteServersSetEventSubscriberMgr::del(::RemoteServersSet::EventListener_ptr oldListener) {
    int error(EXIT_SUCCESS);
    remoteServersSetEventSubscriberMgr::eventsSubscribers::iterator i(find(oldListener));
    if (i != subscribers.end()) {
        writeLockMgr lock(subscribersLock);
        subscriber *item = *i;
        subscribers.erase(i);
        delete item;
    } else {
        WARNING_MSG("subscriber not registered");
        error = ENOENT;
    }
    return error;
}

#ifdef CORBA_INTERFACE

extern "C" int publishEvent(const unsigned long level, const int status, const char *fmt, ...) {
    int error;
    va_list ap;
    remoteServersSetEventSubscriberMgr::eventMsg event;
    
    va_start(ap, fmt);
    event.level = level;
    event.status = status;
    const int messageSize = vsnprintf(event.message, sizeof (event.message), fmt, ap);
    if (messageSize >= 0) {
        event.message[sizeof (event.message) - 1] = '\0';
        DEBUG_MSG("publishing new event (%s;%d) level %d",event.message,event.status,event.level);
        error = theRemoteServersSetEventSubscriberMgr.publish(event);
    } else {
        ERROR_MSG("vsnprintf error (%d)", messageSize);
        error = messageSize;
    }
    va_end(ap);
    return error;
}

#endif /*CORBA_INTERFACE*/
