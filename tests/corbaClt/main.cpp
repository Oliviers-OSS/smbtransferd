#include "config.h"
#include <dbgflags/loggers.h>
#define LOGGER consoleLogger
#define LOG_OPTS 0
#include <dbgflags/debug_macros.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <pthread.h>
#include <orbsvcs/CosNamingC.h>
#include "EventListenerImpl.h"
#include "../../remoteServersSetC.h"
#include <iostream>
#include <string>

errorLogger logError;
debugLogger logDebug;

#undef PACKAGE
#define PACKAGE "smbtransferd"
inline int getRemoteServersSetManagement(CosNaming::NamingContext_var inc,RemoteServersSet::Management_var &remoteServersSetManager) {
    int error(EXIT_SUCCESS);
    try {
        CosNaming::Name name;
        name.length(2);
        name[0].id = CORBA::string_dup(PACKAGE);
        name[0].kind = CORBA::string_dup("");
        name[1].id = CORBA::string_dup("management");
        name[1].kind = CORBA::string_dup("remoteServersSet");

        CORBA::Object_var obj = inc->resolve(name);
        if (!CORBA::is_nil(obj)) {
            remoteServersSetManager = RemoteServersSet::Management::_narrow(obj);
            if (CORBA::is_nil(remoteServersSetManager)) {
                ERROR_STREAM << PACKAGE "/management is nil (wrong type ?)"<< std::endl;
                error = -4;
            }
        } else {
            ERROR_STREAM << PACKAGE "/management is nil !"<< std::endl;
            error = -3;
        }
    } //try
    catch (const CosNaming::NamingContext::NotFound &exception) {
        ERROR_STREAM << "can't find " PACKAGE "/management" << std::endl;
        error = -2;
    }
    catch(const CORBA::Exception &exception) {
        ERROR_STREAM << exception << std::endl;
        error = -1;
    }
    return error;
}

inline int getRemoteServersSetEventSubscriber(CosNaming::NamingContext_var inc,RemoteServersSet::EventSubscriber_var &eventSubscriber) {
    int error(EXIT_SUCCESS);
    try {
        CosNaming::Name name;
        name.length(2);
        name[0].id = CORBA::string_dup(PACKAGE);
        name[0].kind = CORBA::string_dup("");
        name[1].id = CORBA::string_dup("eventSubscriber");
        name[1].kind = CORBA::string_dup("remoteServersSet");

        CORBA::Object_var obj = inc->resolve(name);
        if (!CORBA::is_nil(obj)) {
            eventSubscriber = RemoteServersSet::EventSubscriber::_narrow(obj);
            if (CORBA::is_nil(eventSubscriber)) {
                ERROR_STREAM << PACKAGE "/eventSubscriber is nil (wrong type ?)"<< std::endl;
                error = -4;
            }
        } else {
            ERROR_STREAM << PACKAGE "/eventSubscriber is nil !"<< std::endl;
            error = -3;
        }
    } //try
    catch (const CosNaming::NamingContext::NotFound &exception) {
        ERROR_STREAM << "can't find " PACKAGE "/eventSubscriber" << std::endl;
        error = -2;
    }
    catch(const CORBA::Exception &exception) {
        ERROR_STREAM << exception << std::endl;
        error = -1;
    }
    return error;
}

CORBA::ORB_var orb = NULL;

void *notificationThread(void *param) {
    //CORBA::ORB_var orb = (CORBA::ORB_var)param;
    // run the ORB & wait for notifications
    orb->run();
    return NULL;
}

int main(int argc, char **argv) {
    int error(EXIT_SUCCESS);
    try {
        // init ORB.
        /*CORBA::ORB_var*/ orb = CORBA::ORB_init(argc, argv);

        // activate root POA.
        CORBA::Object_var poa_object = orb->resolve_initial_references("RootPOA");
        PortableServer::POA_var root_poa = PortableServer::POA::_narrow(poa_object);
        PortableServer::POAManager_var poa_manager = root_poa->the_POAManager();
        poa_manager->activate();

        // create corba servants objects.
        EventListenerImpl eventListenerImpl;
        ::RemoteServersSet::EventListener_var eventListener = eventListenerImpl._this();

        // get reference to initial naming context
        CORBA::Object_var obj = orb->resolve_initial_references("NameService");

        CosNaming::NamingContext_var inc = CosNaming::NamingContext::_narrow(obj);
        if (!CORBA::is_nil(inc)) {            
            RemoteServersSet::Management_var remoteServersSetManager;

            error = getRemoteServersSetManagement(inc,remoteServersSetManager);
            if (EXIT_SUCCESS == error) {
                RemoteServersSet::EventSubscriber_var eventSubscriber;
                error = getRemoteServersSetEventSubscriber(inc,eventSubscriber);
                if (EXIT_SUCCESS == error) {
                    // run the ORB & wait for notifications
                    //orb->run();
                    pthread_t notifications;
                    pthread_attr_t attributs;

                    pthread_attr_init(&attributs);
                    pthread_attr_setdetachstate(&attributs,PTHREAD_CREATE_DETACHED);
                    error = pthread_create(&notifications,&attributs,notificationThread,NULL);
                    pthread_attr_destroy(&attributs);
                    if (EXIT_SUCCESS == error) {
                        bool doContinue(true);
                        unsigned int selection;
                        do{
                            std::cout << std::endl << std::endl << "available cmds :"<< std::endl;
                            std::cout << "1:\t start smbtransfertd"<< std::endl;
                            std::cout << "2:\t get currentServer"<< std::endl;
                            std::cout << "3:\t set currentServer"<< std::endl;
                            std::cout << "4:\t get autoMoveMode"<< std::endl;
                            std::cout << "5:\t set autoMoveMode"<< std::endl;
                            std::cout << "6:\t get currentCommStatus"<< std::endl;
                            std::cout << "7:\t subscribe for notification"<< std::endl;
                            std::cout << "8:\t unsubscribe for notification"<< std::endl;
                            std::cout << "9:\t end program"<< std::endl;
                            std::cout << std::endl << "please select:";

                            std::cin >> selection;
                            try {
                                switch(selection) {
                                    case 1: {
                                            const CORBA::Long status = remoteServersSetManager->start();
                                            DEBUG_STREAM << "remoteServersSetManager->start() status = " << status << std::endl;
                                        }
                                        break;
                                    case 2: {
                                            const CORBA::String_var currentServer(remoteServersSetManager->currentServer());
                                            DEBUG_STREAM << "currentServer = " << currentServer << std::endl;
                                        }
                                        break;
                                    case 3: {                                            
                                            std::string newServer("192.168.162.128");
                                            //std::cout << std::endl << "new server = ";
                                            //std::getline(std::cin,newServer);
                                            const CORBA::String_var server(newServer.c_str());
                                            remoteServersSetManager->currentServer(server);
                                       }
                                        break;
                                    case 4: {
                                            const CORBA::Boolean autoMoveMode = remoteServersSetManager->autoMoveMode();
                                            DEBUG_STREAM << "autoMoveMode = " << autoMoveMode << std::endl;
                                        }
                                        break;
                                    case 5: {
                                        std::string autoMoveModeString;

                                            std::cout << std::endl << "autoMoveMode = ";
                                            std::getline(std::cin,autoMoveModeString);
                                            const CORBA::Boolean autoMoveMode((strcasecmp(autoMoveModeString.c_str(),"true") == 0) || (strcasecmp(autoMoveModeString.c_str(),"1") == 0));
                                            remoteServersSetManager->autoMoveMode(autoMoveMode);
                                        }
                                        break;
                                    case 6: {
                                            const CORBA::Long commStatus(remoteServersSetManager->currentCommStatus());
                                            DEBUG_STREAM << "current CommStatus = " << commStatus << "(" << strerror(commStatus) << ")" << std::endl;
                                        }
                                        break;
                                    case 7: {
                                            const CORBA::ULong status = eventSubscriber->subscribe(eventListener,RemoteServersSet::EventSubscriber::informationEventFilters|RemoteServersSet::EventSubscriber::warningsEventFilters|RemoteServersSet::EventSubscriber::errorsEventFilters);
                                            DEBUG_STREAM << "subscribe status = " << status << std::endl;
                                        }
                                        break;
                                    case 8: {
                                            const CORBA::ULong status = eventSubscriber->unsubscribe(eventListener);
                                            DEBUG_STREAM << "unsubscribe status = " << status << std::endl;
                                        }
                                        break;
                                    case 9:
                                        doContinue = false;
                                        break;
                                    default:
                                        std::cout << "invalid selection " << selection <<std::endl;
                                } //switch(selection)
                            } // try
                            catch (RemoteServersSet::error &exception) {
                                ERROR_STREAM << "RemoteServersSet::error " << exception.code << "(" << strerror(exception.code) << ")" << std::endl;
                            }
                            catch(const CORBA::UserException &exception) {
                                ERROR_STREAM << "CORBA::UserException " << exception << std::endl;
                                exception._tao_print_exception("");                                
                            }
                            catch (const CORBA::SystemException &exception) {
                                ERROR_STREAM << "CORBA::SystemException " << exception << std::endl;
                                exception._tao_print_exception("");
                            }
                            catch (const CORBA::Exception &exception) {
                                ERROR_STREAM << "CORBA::Exception " << exception << std::endl;
                                exception._tao_print_exception("");                                
                            }
                        } while(doContinue);
                    } else {
                        ERROR_STREAM << "pthread_create error " << error << std::endl;
                    }
                } else {
                    ERROR_STREAM << "can't get cnx to eventSubscriber servant (" << error << ")" << std::endl;
                }
            } else {
                ERROR_STREAM << "can't get cnx to remoteServersSetManager servant (" << error << ")" << std::endl;
            }
        } else { //(!CORBA::is_nil(inc))
            ERROR_STREAM << "ORB configuration error" << std::endl;
            error = -2;
        }
        // clean up.
        root_poa->destroy(1, 1);
        orb->destroy();
    }//try
    catch (RemoteServersSet::error &exception) {
        ERROR_STREAM << "RemoteServersSet::error " << exception.code << std::endl;
    }
    catch (const CORBA::Exception &exception) {
        ERROR_STREAM << "CORBA::Exception " << exception << std::endl;
        exception._tao_print_exception("");
        error = -1;
    }
    catch (...) {
       ERROR_STREAM << "Exception " << std::endl;
    }
    return error;
}
