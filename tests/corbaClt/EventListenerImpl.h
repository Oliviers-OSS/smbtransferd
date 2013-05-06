/* 
 * File:   EventListenerImpl.h
 * Author: oc
 *
 * Created on January 8, 2011, 9:08 PM
 */

#ifndef _EVENTLISTENERIMPL_H
#define	_EVENTLISTENERIMPL_H

#include "../../remoteServersSetS.h"

class EventListenerImpl : public POA_RemoteServersSet::EventListener {
public:
    EventListenerImpl();
    EventListenerImpl(const EventListenerImpl& orig);
    virtual ~EventListenerImpl();

    // CORBA interface methods
    virtual void notifyEvent (const ::RemoteServersSet::Event & newEvent) ACE_THROW_SPEC ((CORBA::SystemException));
};

#endif	/* _EVENTLISTENERIMPL_H */

