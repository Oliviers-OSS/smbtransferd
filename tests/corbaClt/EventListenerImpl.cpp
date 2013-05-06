/* 
 * File:   EventListenerImpl.cpp
 * Author: oc
 * 
 * Created on January 8, 2011, 9:08 PM
 */

#include "EventListenerImpl.h"
#include <dbgflags/loggers.h>
//#define LOGGER consoleLogger
//#define LOG_OPTS 0
#include <dbgflags/debug_macros.h>
#include <cstdlib>
#include <cstdio>

extern errorLogger logError;
extern debugLogger logDebug;

EventListenerImpl::EventListenerImpl() {
    DEBUG_STREAM << "EventListenerImpl build " << std::endl;
}

EventListenerImpl::EventListenerImpl(const EventListenerImpl& orig) {
    ERROR_STREAM << "EventListenerImpl copy " << std::endl;
}

EventListenerImpl::~EventListenerImpl() {
    DEBUG_STREAM << "EventListenerImpl destroyed " << std::endl;
}

void EventListenerImpl::notifyEvent (const ::RemoteServersSet::Event & newEvent) ACE_THROW_SPEC ((CORBA::SystemException)){
    DEBUG_STREAM << "new event: " << static_cast<const char*>(newEvent.message) << "status = " << newEvent.status << std::endl;
    fprintf(stderr,"new event: %s (status = %d)\n",(const char*)newEvent.message,newEvent.status);
}
