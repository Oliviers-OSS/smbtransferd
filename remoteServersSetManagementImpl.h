/* 
 * File:   remoteServersSetImpl.h
 * Author: oc
 *
 * Created on November 12, 2010, 9:07 AM
 */

#ifndef _REMOTESERVERSSETIMPL_H_
#define	_REMOTESERVERSSETIMPL_H_

#include "remoteServersSetS.h"

class remoteServersSetManagementImpl : public POA_RemoteServersSet::Management{
public:
    remoteServersSetManagementImpl() ACE_THROW_SPEC ((CORBA::SystemException));
    remoteServersSetManagementImpl(const remoteServersSetManagementImpl& orig) ACE_THROW_SPEC ((CORBA::SystemException));
    virtual ~remoteServersSetManagementImpl();

    // CORBA interface methods
    virtual char* currentServer () ACE_THROW_SPEC ((CORBA::SystemException));
    virtual void currentServer (const char * currentServer) ACE_THROW_SPEC ((CORBA::SystemException));
    
    virtual CORBA::Boolean autoMoveMode () ACE_THROW_SPEC ((CORBA::SystemException));
    virtual void autoMoveMode (::CORBA::Boolean autoMoveMode) ACE_THROW_SPEC ((CORBA::SystemException));

    virtual CORBA::Long currentCommStatus () ACE_THROW_SPEC ((CORBA::SystemException));

    virtual CORBA::Long start () ACE_THROW_SPEC ((CORBA::SystemException));


private:

};

#endif	/* _REMOTESERVERSSETIMPL_H_ */

