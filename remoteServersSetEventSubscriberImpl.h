/* 
 * File:   remoteServersSetEventSubscriberImpl.h
 * Author: oc
 *
 * Created on November 12, 2010, 9:14 AM
 */

#ifndef _REMOTESERVERSSETEVENTSUBSCRIBERIMPL_H
#define	_REMOTESERVERSSETEVENTSUBSCRIBERIMPL_H

#include "remoteServersSetS.h"

class remoteServersSetEventSubscriberImpl : public POA_RemoteServersSet::EventSubscriber {

public:
    remoteServersSetEventSubscriberImpl();
    remoteServersSetEventSubscriberImpl(const remoteServersSetEventSubscriberImpl& orig);
    virtual ~remoteServersSetEventSubscriberImpl();

    // CORBA interface methods
    virtual CORBA::ULong subscribe (::RemoteServersSet::EventListener_ptr clientsInterface,::CORBA::ULong filter) ACE_THROW_SPEC ((CORBA::SystemException));
    virtual CORBA::ULong unsubscribe (::RemoteServersSet::EventListener_ptr clientsInterface) ACE_THROW_SPEC ((CORBA::SystemException));   

};

#endif	/* _REMOTESERVERSSETEVENTSUBSCRIBERIMPL_H */

