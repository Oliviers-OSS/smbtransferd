/* 
 * File:   remoteServersSetImpl.cpp
 * Author: oc
 * 
 * Created on November 12, 2010, 9:07 AM
 */

#include "remoteServersSetManagementImpl.h"
#include "globals.h"
#include "configuration.h"
#include "runtime.h"

#define MODULE_FLAG FLAG_CORBA

remoteServersSetManagementImpl::remoteServersSetManagementImpl() ACE_THROW_SPEC ((CORBA::SystemException)) {
}

remoteServersSetManagementImpl::remoteServersSetManagementImpl(const remoteServersSetManagementImpl& orig) ACE_THROW_SPEC ((CORBA::SystemException)){
}

remoteServersSetManagementImpl::~remoteServersSetManagementImpl() {
}

char* remoteServersSetManagementImpl::currentServer ()  ACE_THROW_SPEC ((CORBA::SystemException)) {
    const char *currentServer = getCurrentServer(&configuration,NULL);
    DEBUG_VAR(currentServer,"%s");
    return CORBA::string_dup(const_cast<char*>(currentServer));
}

void remoteServersSetManagementImpl::currentServer (const char * currentServer)  ACE_THROW_SPEC ((CORBA::SystemException)) {
   const int error(setCurrentServer(&configuration,currentServer));
   DEBUG_VAR(error,"%d");
   if (error != EXIT_SUCCESS) {
       throw RemoteServersSet::error(error);
   }
}
#define _T(x) x
CORBA::Boolean remoteServersSetManagementImpl::autoMoveMode ()  ACE_THROW_SPEC ((CORBA::SystemException)) {
    CORBA::Boolean currentMode(isChangeServerAutoModeSet(configuration) == 1);
    DEBUG_VAR_BOOL(currentMode);
    return currentMode;
}

void remoteServersSetManagementImpl::autoMoveMode (::CORBA::Boolean autoMoveMode)  ACE_THROW_SPEC ((CORBA::SystemException)) {
    DEBUG_VAR_BOOL(autoMoveMode);
    if (autoMoveMode) {
        setChangeServerAutoMode(configuration);
    } else {
        unsetChangeServerAutoMode(configuration);
    }
}

CORBA::Long remoteServersSetManagementImpl::currentCommStatus () ACE_THROW_SPEC ((CORBA::SystemException)) {
    CORBA::Long currentStatus(EXIT_SUCCESS);
    DEBUG_VAR(currentStatus,"%d");
    return currentStatus;
}

CORBA::Long remoteServersSetManagementImpl::start () ACE_THROW_SPEC ((CORBA::SystemException)) {
   CORBA::Long  error(EXIT_SUCCESS);
   resumeDaemonStartup();
   return error;
}
