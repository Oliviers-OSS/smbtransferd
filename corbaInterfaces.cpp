#include <malloc.h>
#include "globals.h"
#include "remoteServersSetS.h"
#include "remoteServersSetManagementImpl.h"
#include "remoteServersSetEventSubscriberImpl.h"
#include <orbsvcs/CosNamingC.h>
#include "ModuleVersionInfo.h"

#define MODULE_FLAG FLAG_CORBA

//emergencyLogger logEmergency;
//alertLogger logAlert;
//criticalLogger logCritical;
errorLogger logError;
//warningLogger logWarning;
//noticeLogger logNotice;
//infoLogger logInfo;
debugLogger logDebug;
//contextLogger logContext;

static inline void registerServants(CosNaming::NamingContext_var initialNamingContext, ::RemoteServersSet::Management_var managementServant,::RemoteServersSet::EventSubscriber_var eventSubscriber ) {
    if (!CORBA::is_nil(initialNamingContext)) {
        CosNaming::Name name;

        // create the smbtransferd naming context
        name.length(1);
        name[0].id = CORBA::string_dup(PACKAGE);
        name[0].kind = CORBA::string_dup("");
        CosNaming::NamingContext_var smbtransferdNamingContext = initialNamingContext->new_context();
        initialNamingContext->rebind_context(name, smbtransferdNamingContext);
        DEBUG_STREAM << "smbtransferd naming context created" << std::endl;

        // register the server in their naming context.
        name.length(2);
        name[1].id = CORBA::string_dup("management");
        name[1].kind = CORBA::string_dup(CORBA_NS_KIND);
        initialNamingContext->rebind(name, managementServant);
        DEBUG_STREAM << "smbtransferd's management registered in the Naming Service"<< std::endl;

        name[1].id = CORBA::string_dup("eventSubscriber");
        name[1].kind = CORBA::string_dup(CORBA_NS_KIND);
        initialNamingContext->rebind(name, eventSubscriber);
        DEBUG_STREAM << "smbtransferd's eventSubscriber registered in the Naming Service"<< std::endl;
    } else {
        ERROR_STREAM << "invalid initialNamingContext"<< std::endl;
    }
}

static inline void unregisterServants(CosNaming::NamingContext_var initialNamingContext) {
    if (!CORBA::is_nil(initialNamingContext)) {
        CosNaming::Name name;

        name.length(2);
        name[0].id = CORBA::string_dup(PACKAGE);
        name[0].kind = CORBA::string_dup("");
        name[1].id = CORBA::string_dup("management");
        name[1].kind = CORBA::string_dup(CORBA_NS_KIND);
        initialNamingContext->unbind(name);
        DEBUG_STREAM << "smbtransferd's management unregistered in the Naming Service"<< std::endl;

        name.length(2);
        name[1].id = CORBA::string_dup("eventSubscriber");
        name[1].kind = CORBA::string_dup(CORBA_NS_KIND);
        initialNamingContext->unbind(name);
        DEBUG_STREAM << "smbtransferd's eventSubscriber unregistered in the Naming Service"<< std::endl;
    } else {
        ERROR_STREAM << "invalid initialNamingContext"<< std::endl;
    }
}

static inline int startRemoteServersSetInterfaces(int argc, char **argv) {
    int error(EXIT_SUCCESS);
    try {
        // init ORB.
        CORBA::ORB_var orb = CORBA::ORB_init(argc, argv);

        // activate root POA.
        CORBA::Object_var poa_object = orb->resolve_initial_references("RootPOA");
        PortableServer::POA_var root_poa = PortableServer::POA::_narrow(poa_object);
        PortableServer::POAManager_var poa_manager = root_poa->the_POAManager();
        poa_manager->activate();

        // create corba servants objects.
        remoteServersSetManagementImpl managementImpl;
        ::RemoteServersSet::Management_var managementServant = managementImpl._this();

        remoteServersSetEventSubscriberImpl eventSubscriberImpl;
        ::RemoteServersSet::EventSubscriber_var eventSubscriber = eventSubscriberImpl._this();
        
        try { //else the process will crash in case of NameService failure because of the ORB hasn't been cleanup
            // get reference to naming service.
            CORBA::Object_var initialNamingContextObject = orb->resolve_initial_references("NameService");
            if (!CORBA::is_nil(initialNamingContextObject)) {
                CosNaming::NamingContext_var initialNamingContext = CosNaming::NamingContext::_narrow(initialNamingContextObject);
                if (!CORBA::is_nil(initialNamingContext)) {

                    registerServants(initialNamingContext,managementServant,eventSubscriber);

                    // run the ORB & wait for clients
                    orb->run();

                    unregisterServants(initialNamingContext);
                } else {
                    ERROR_STREAM <<"naming_context NameService is nil !"<< std::endl;
                    error = EPROTO;
                }
            } else {
                ERROR_STREAM <<"resolve_initial_references NameService is nil !"<< std::endl;
                error = EPROTO;
            }
        } //try
        catch (CORBA::Exception & ex) {
            ERROR_STREAM << "CORBA::Exception: " << ex << std::endl;
            error = EIO; //TODO: set better (real ?) error code number
        }
        catch(...) {
            ERROR_STREAM <<"catch all(2): exception caught !"<< std::endl;
            error = EFAULT;
        }
        // clean up.
        root_poa->destroy(1, 1);
        orb->destroy();

    }//try
    catch (CORBA::Exception & ex) {
        ERROR_STREAM << "CORBA::Exception: " << ex << std::endl;
        error = EIO; //TODO: set better (real ?) error code number
    }    catch (...) {
        ERROR_STREAM << "catch all: exception caught !" << std::endl;
        error = EFAULT;
    }

    DEBUG_VAR(error, "%d");
    return error;
}

extern "C" {

    int startCORBAInterfaces(int argc, char **argv) {
        const int error = startRemoteServersSetInterfaces(argc,argv);
        return error;
    }
    
}

MODULE_FILE_VERSION(Corba Interface 1.0);
