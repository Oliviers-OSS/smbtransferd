/* 
 * File:   remoteServersSetEventSubscriberImpl.cpp
 * Author: oc
 * 
 * Created on November 12, 2010, 9:14 AM
 */

#include "remoteServersSetEventSubscriberImpl.h"
#include "globals.h"
#include "remoteServersSetEventSubscriberMgr.h"

#define MODULE_FLAG FLAG_CORBA

remoteServersSetEventSubscriberImpl::remoteServersSetEventSubscriberImpl() {
}

remoteServersSetEventSubscriberImpl::remoteServersSetEventSubscriberImpl(const remoteServersSetEventSubscriberImpl& orig) {
}

remoteServersSetEventSubscriberImpl::~remoteServersSetEventSubscriberImpl() {
}

CORBA::ULong remoteServersSetEventSubscriberImpl::subscribe (::RemoteServersSet::EventListener_ptr clientsInterface,::CORBA::ULong filter) ACE_THROW_SPEC ((CORBA::SystemException)) {
   CORBA::ULong error(theRemoteServersSetEventSubscriberMgr.add(clientsInterface,filter));
   DEBUG_VAR(error,"%d");
   return error;
}

CORBA::ULong remoteServersSetEventSubscriberImpl::unsubscribe (::RemoteServersSet::EventListener_ptr clientsInterface) ACE_THROW_SPEC ((CORBA::SystemException)) {
   CORBA::ULong error(theRemoteServersSetEventSubscriberMgr.del(clientsInterface));
   DEBUG_VAR(error,"%d");
   return error;
}


