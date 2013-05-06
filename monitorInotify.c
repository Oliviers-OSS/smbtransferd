#include "globals.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
/*#include "inotify-nosys.h"*/
#include <sys/select.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <linux/stddef.h>

#define MODULE_FLAG FLAG_MONITOR 

#ifdef _DEBUG_
//#define MONITORED_EVENTS IN_ALL_EVENTS /*IN_MOVED_TO*/
static inline void printEvent(const unsigned int event) {
    DEBUG_VAR(event,"0x%X");
    if (event & IN_ACCESS) {          
        DEBUG_MSG("File was accessed");
    }
    if (event & IN_MODIFY) {          
        DEBUG_MSG("File was modified");
    }
    if (event & IN_ATTRIB) {          
        DEBUG_MSG("Metadata changed");
    }
    if (event & IN_CLOSE_WRITE) {     
        DEBUG_MSG("Writtable file was closed");
    }
    if (event & IN_CLOSE_NOWRITE) {   
        DEBUG_MSG("Unwrittable file closed");
    }
    if (event & IN_OPEN) {            
        DEBUG_MSG("File was opened");
    }
    if (event & IN_MOVED_FROM) {      
        DEBUG_MSG("File was moved from X");
    }
    if (event & IN_MOVED_TO) {        
        DEBUG_MSG("File was moved to Y"); /* rename target */
    }
    if (event & IN_CREATE) {          
        DEBUG_MSG("Subfile was created");
    }
    if (event & IN_DELETE) {          
        DEBUG_MSG("Subfile was deleted");
    }
    if (event & IN_DELETE_SELF) {     
        DEBUG_MSG("Self was deleted");
    }
    if (event & IN_MOVE_SELF) {       
        DEBUG_MSG("Self was moved");
    }

    /* the following are legal events.  they are sent as needed to any watch */
    if (event & IN_UNMOUNT) {        
        DEBUG_MSG("Backing fs was unmounted");
    }
    if (event & IN_Q_OVERFLOW) {     
        DEBUG_MSG("Event queued overflowed");
    }
    if (event & IN_IGNORED) {        
        DEBUG_MSG(" File was ignored");
    }
}
#define DEBUG_PRINT_EVENT(x) printEvent(x)
#else /* _DEBUG_ */
//#define MONITORED_EVENTS IN_MOVED_TO
#define DEBUG_PRINT_EVENT(x)
#endif /* _DEBUG_ */

#define SIG_STOP_MONITORING SIGUSR1
int stopMonitoring(pthread_t thread) {
   int error = pthread_kill(thread,SIG_STOP_MONITORING);
  return error;
}

int monitorInotify(const char *directory,const unsigned int monitoredEvent,onInotifyEvent_t onEvent,void *onEventParameter) {
    int error = EXIT_SUCCESS;
    int inotify_fd = inotify_init();
    if (inotify_fd >0) {
        DEBUG_VAR(directory,"%s");
        int wd =  inotify_add_watch(inotify_fd,directory,monitoredEvent);
        if (wd > 0) {	   
            unsigned int stop = 0;
            int calledError = EXIT_SUCCESS;
            fd_set setOfFileDescriptors;
	    sigset_t blockedSignalsMask;
	    
            FD_ZERO(&setOfFileDescriptors);
            FD_SET(inotify_fd,&setOfFileDescriptors);
	    
	    sigemptyset(&blockedSignalsMask);
            sigaddset(&blockedSignalsMask,SIGHUP);
            #ifdef _DEBUG_
            //sigaddset(&blockedSignalsMask,SIGINT);
            //sigaddset(&blockedSignalsMask,SIGQUIT);
            //sigaddset(&blockedSignalsMask,SIGTERM);
            #endif /* _DEBUG_ */

           do
           {
             int n = 0;
             DEBUG_MSG("waiting for events (0x%X)...",monitoredEvent);
	     n = pselect(FD_SETSIZE,&setOfFileDescriptors,NULL,NULL,NULL,&blockedSignalsMask);
             //n = select(FD_SETSIZE,&setOfFileDescriptors,NULL,NULL,NULL);
             DEBUG_VAR(n,"%d");
            if (n > 0) {
		char buffer[PATH_MAX + sizeof(struct inotify_event)];
                char name[PATH_MAX]; // TODO: manage a map pevent-name and not only one name (but when deallocate it ?)
                unsigned int nameLenght = 0;
                ssize_t r = read(inotify_fd,buffer,sizeof(buffer));
                if (r >= 0) {
                    char *cursor = buffer;
                    char *limit = buffer + r;
                    name[0] = '\0';
                    nameLenght = 0;
                    while(cursor < limit) {
                        struct inotify_event *pevent = (struct inotify_event*)cursor;
                        DEBUG_VAR(pevent,"0x%X");
                        size_t event_size = offsetof(struct inotify_event,name) + pevent->len;
                        DEBUG_VAR(event_size,"%d");
                        DEBUG_VAR(pevent->len,"%d");
                        if (pevent->len) {
                            DEBUG_VAR(pevent->name,"%s");                            
                            strcpy(name,pevent->name);
                            nameLenght = pevent->len;
                        }
                        DEBUG_PRINT_EVENT(pevent->mask);

                        if (onEvent != NULL) {
                           error = (*onEvent)(name,nameLenght,pevent->mask,onEventParameter);
                           DEBUG_VAR(error,"%d");
                        } else {
                            DEBUG_MSG("onEvent fct ptr is not set !");
                        }

                        cursor += event_size;
		    } /* while(cursor < limit)*/
		} else { /* read(inotify_fd < 0 */
                    error = errno;
		    ERROR_MSG("read inotify_fd error %d (%m)",error);
		    if (EINTR == error) {
			stop = 1;
			DEBUG_MSG("signal received during read");
		    } else {
			ERROR_MSG("read inotify_fd error %d (%m)",error);
		    }
                }
            } else { /* !(n > 0) */
                error = errno;
		if (EINTR == error) {
			stop = 1;
			DEBUG_MSG("signal received during select");
		} else {
                   ERROR_MSG("pselect error %d (%m)",error);
		}
            }
	   } while (!stop);
        } else {
            error = -wd;
            ERROR_MSG("inotify_add_watch error %d",wd);
        }
    } else {
        error = -inotify_fd;
        ERROR_MSG("inotify_init error %d",inotify_fd);
    }
    return error;
}
