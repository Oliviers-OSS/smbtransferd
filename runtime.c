#include "globals.h"
#include "messagesQueue.h"
#include "filesystem.h"
#include "threadsPool.h"
#include <sys/types.h>
#include <pthread.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define MODULE_FLAG FLAG_RUNTIME

#define  MESSAGE_QUEUE_NAME "/"PACKAGE


/*struct AllocatedRessources {
} allocatedRessources;
 */

unsigned int global_command = CMD_NOTHING;

void sigHupSignalHandler(int receivedSignal) {
    int error = EXIT_SUCCESS;
    NOTICE_MSG("SIGHUP received, updating configuration");
    error = readConfiguration(); /* error already logged */
}

void sigTermSignalHandler(int receivedSignal) {
    int error = EXIT_SUCCESS;
    NOTICE_MSG("SIGTERM received, exiting...");
    global_command = CMD_STOP;
    //exit(error);
}

void sigQuitSignalHandler(int receivedSignal) {
    int error = EXIT_SUCCESS;
    NOTICE_MSG("SIGQUIT received, exiting...");
    global_command = CMD_STOP;
    //exit(error);
}

static inline int disabledSignals(void) {
    int error = EXIT_SUCCESS;
    sigset_t newMask, oldMask;

    sigemptyset(&newMask);
    sigaddset(&newMask, SIGHUP);
    sigaddset(&newMask, SIGTERM);
    sigaddset(&newMask, SIGQUIT);
    error = pthread_sigmask(SIG_BLOCK, &newMask, &oldMask);
    if (error != 0) {
        ERROR_MSG("pthread_sigmask error (d)", error);
    }

    return error;
}

static inline unsigned int moveToNextServer(const int error) {
    unsigned int move = 0;
    if (isChangeServerAutoModeSet(configuration)) { /* don't lock because this field is only use during configuration */
        switch (error) {
            case ETIMEDOUT:
                /* break is missing */
            case ENODATA:
                /* break is missing */
            case EACCES:
                move = 1;
                NOTICE_MSG("moving to the next server (because of error = %d)",error);
                break;
            default:
                NOTICE_MSG("error %d doesn't imply moving to a new server",error);
        } /* switch(error) */
    } else { /* (isChangeServerAutoModeSet(configuration)) */
        NOTICE_MSG("current configuration doesn't allow to move to a new server");
    }
    return move;
}

static void* resume(void *argument) {
    int error = EXIT_SUCCESS;
    const char *srcDirectory = getSourceDirectoryName(configuration);
    const char *dstDirectory = getDestinationDirectoryName(configuration);
    struct timeval currentServerStartOfUse;
    DIR *directory = NULL;

    disabledSignals(); /* error already logged, anyway go on */

    /* remove all *.done directories */
    directory = opendir(srcDirectory);
    if (directory != NULL) {
        struct dirent *directoryEntry = NULL;
        while ((directoryEntry = readdir(directory)) != NULL) {
            DEBUG_VAR(directoryEntry->d_name, "%s");
            DEBUG_VAR(directoryEntry->d_type, "%d");
            if ((DT_REG == directoryEntry->d_type) && (directoryEntry->d_name[0] != '.')) {
                const char* extension = strrchr(directoryEntry->d_name, '.');
                if (extension != NULL) {
                    DEBUG_VAR(extension, "%s");
                    if (strcmp(extension, COMPLETED_EXTENSION) == 0) {
                        error = removeDirectory(directoryEntry->d_name, 1); /* error already logged */
                    }
                } else {
                    DEBUG_MSG("%s skipped", directoryEntry->d_name);
                }
            }
        } /* ((directoryEntry = readdir(directory)) != NULL) */
        if (closedir(directory) != 0) {
            error = errno;
            ERROR_MSG("closedir %s for resuming error %d (%m)", srcDirectory, error);
        }
    } else {
        error = errno;
        ERROR_MSG("opendir %s for resuming error %d (%m)", srcDirectory, error);
    }

    /* resend all remote *.work directories */
    error = smbResumeCopy(srcDirectory, getCurrentServer(&configuration, &currentServerStartOfUse), dstDirectory); /* error already logged */
    if (moveToNextServer(error)) {        
        error = changeCurrentServer(&configuration, &currentServerStartOfUse);
        if (EXIT_SUCCESS == error) {
            error = smbResumeCopy(srcDirectory, getCurrentServer(&configuration, &currentServerStartOfUse), dstDirectory); /* try again using this new server */
        }
    }
    return NULL;
}

static int transfertFct(const char *directoryName) {
    int error = EXIT_SUCCESS;
    const char *srcDirectory = getSourceDirectoryName(configuration);

    disabledSignals(); /* error already logged, anyway go on */

    if (chdir(srcDirectory) != -1) {
        const char *destinationDirectory = getDestinationDirectoryName(configuration);
        struct timeval currentServerStartOfUse;
        const char *currentServer = getCurrentServer(&configuration, &currentServerStartOfUse);

        error = smbCopyDirectory(directoryName, currentServer, destinationDirectory);
        if (error != EXIT_SUCCESS) {            
            DEBUG_VAR(error, "%d");
            if (moveToNextServer(error)) {
                error = changeCurrentServer(&configuration, &currentServerStartOfUse);
                if (EXIT_SUCCESS == error) {
                    error = transfertFct(directoryName); /* try again using this new server */
                } /* error already logged */
            }
        }
    } else {
        error = errno;
        ERROR_MSG("chdir %s error %d (%m)", srcDirectory,error);
    }

    return error;
}

void* transfertMgr(void *argument) {
    messageQueueId fromNotifier = -1;
    int error = createMessagesQueue(MESSAGE_QUEUE_NAME, O_RDONLY, &fromNotifier);
    if (EXIT_SUCCESS == error) {
        threadPool_t threadPool;
        error = threadPoolCreate(&threadPool, THREADS_POOL_SIZE, transfertFct);
        if (EXIT_SUCCESS == error) {
            while (global_command != CMD_STOP) {
                /* wait for an available worker thread */
                threadData_t *workerThread = threadpoolGetAvailable(&threadPool);
                if (workerThread != NULL) {
                    /* wait for msg (TODO remove the message copy) */
                    char message[8192]; /* WARNING: this value can't be changed else the read will fail */
                    size_t messageLength = sizeof (message);
                    unsigned int messagePriority = 0;
                    error = readFromMessagesQueue(fromNotifier, message, &messageLength, &messagePriority);
                    if (EXIT_SUCCESS == error) {
                        error = threadDataSetWork(workerThread, message); /* error already logged */
                    } /* error already logged */
                } else {
                    INFO_MSG("BUG: semaphore acquired but no worker thread available found");
                }
            } /* (global_command != CMD_STOP)*/
        } /* (EXIT_SUCCESS == error) */
        closeMessagesQueue(fromNotifier);
        fromNotifier = -1;
    } else {
        /* error already logged */
    }
    return NULL;
}

int onEvent(const char *name, unsigned int nameLength, unsigned int events, void *parameter) {
    int error = EINVAL;
    if ((name[0] != '\0') && (events & IN_MOVED_TO)) {
        messageQueueId notifierToSender = (messageQueueId) parameter;
        const char *extension = strrchr(name, '.');
        if (extension != NULL) {
            if ((strcmp(extension, IN_PROGRESS_EXTENSION) != 0) && (strcmp(extension, COMPLETED_EXTENSION) != 0)) {
                error = writeToMessagesQueue(notifierToSender, name, nameLength, 0);
                DEBUG_VAR(error, "%d");
            } else {
                DEBUG_MSG("bad extension (%s)", extension);
            }
        } else {
            DEBUG_MSG("extension is NULL");
            error = writeToMessagesQueue(notifierToSender, name, nameLength, 0);
        }
        DEBUG_VAR(name, "%s");
    } else {
        DEBUG_MSG("onEvent called with bad event (0x%X) and/or invalid filename (%s)", events, name);
    }
    return error;
}

void* monitor(void *argument) {
    messageQueueId notifierToSender = -1;
    int error = createMessagesQueue(MESSAGE_QUEUE_NAME, O_WRONLY, &notifierToSender);
    if (EXIT_SUCCESS == error) {
        const char *srcDirectory = getSourceDirectoryName(configuration);
        error = monitorInotify(srcDirectory, IN_MOVED_TO, onEvent, (void *) notifierToSender);
        closeMessagesQueue(notifierToSender);
        notifierToSender = -1;
    } else {
        /* error already logged */
    }
    return NULL;
}

#ifdef CORBA_INTERFACE

typedef enum threadNames_ {
    CorbaInterface = 0
    , Samba
    , Notifier
    , NumberOfThreads
} threadNames;

typedef struct cmdLine_ {
    int argc;
    char **argv;
} cmdLine;

static void* CORBAInterfacesThread(void *arguments) {
    cmdLine *arg = (cmdLine *)arguments;
    const int error = startCORBAInterfaces(arg->argc,arg->argv);
    if (error != EXIT_SUCCESS) {
        /* fatal error  */
       CRIT_MSG("CORBA interface error %d", error);
       exit(-error);
    }
    return NULL;
}

#else /* CORBA_INTERFACE */

typedef enum threadNames_ {
    Samba = 0
    , Notifier
    , NumberOfThreads
} threadNames;
#endif /* CORBA_INTERFACE */

static event_t *suspendEventPtr = NULL;

int resumeDaemonStartup() {
    int error = EXIT_SUCCESS;
    if (suspendEventPtr != NULL) {
        error = setEvent(suspendEventPtr);
    }
    return error;
}

static void resumeStartup(int receivedSignal) {
    if (SIGUSR1 == receivedSignal) {
        resumeDaemonStartup();
    }
}

int run(int argc, char** argv) {
    int error = EXIT_SUCCESS;
    pthread_t threads[NumberOfThreads];
    const char *srcDirectory = getSourceDirectoryName(configuration);
    const char *dstDirectory = getDestinationDirectoryName(configuration);

    REGISTER_DBGFLAGS(debugFlags);
    NOTICE_MSG(PACKAGE_NAME " (v" PACKAGE_VERSION ") build on the " __DATE__ " " __TIME__ " started");

    if (signal(SIGQUIT, sigQuitSignalHandler) == SIG_ERR) {
        ERROR_MSG("error setting signal SIGQUIT handler");
    }

    if (srcDirectory[0] != '\0') {

        if (dstDirectory[0] != '\0') {
            pthread_attr_t attributs;

#ifdef CORBA_INTERFACE
            cmdLine args = {argc,argv};
            pthread_attr_init(&attributs);
            pthread_attr_setdetachstate(&attributs, PTHREAD_CREATE_DETACHED);
            error = pthread_create(&threads[CorbaInterface], &attributs, CORBAInterfacesThread, &args);
            pthread_attr_destroy(&attributs);
            if (EXIT_SUCCESS == error) {
#endif /* CORBA_INTERFACE */

                if (isSuspendOnStartupSet(configuration)) {
                    event_t suspendEvent;
                    error = createEvent(&suspendEvent,0);
                    if (EXIT_SUCCESS == error) {
                        suspendEventPtr =&suspendEvent;
                        if (signal(SIGUSR1, resumeStartup) != SIG_ERR) {
#ifndef CORBA_INTERFACE
                            WARNING_MSG("SuspendOnStartup set without CORBA interface to wake up (SIGUSR1 signal is still available)");
#endif /* CORBA_INTERFACE */
                            if (EXIT_SUCCESS == error) {
                                NOTICE_MSG("waiting for startup event...");
                                error = waitForEvent(suspendEventPtr);
                                NOTICE_MSG("startup event received (%d), starting monitoring...",error);
                            } //errors already printed and managed by called functions
                        } else {
                            ERROR_MSG("error setting signal SIGUSR1 handler for resuming suspended startup");
                        }
                        suspendEventPtr = NULL;
                        destroyEven(&suspendEvent);
                    } else {
                        ERROR_MSG("SuspendOnStartup init error (%d)",error);
                    }
                } /* (isSuspendOnStartupSet(configuration)) */
            
                if (isResumeOnStartupSet(configuration)) {
                    pthread_t pthreadsResume;                    

                    pthread_attr_init(&attributs);
                    pthread_attr_setdetachstate(&attributs, PTHREAD_CREATE_DETACHED);
                    error = pthread_create(&pthreadsResume, &attributs, resume, NULL);
                    if (EXIT_SUCCESS == error) {
                        NOTICE_MSG("resume in progress...");
                    } else {
                        ERROR_MSG("pthread_create resume transferts error (%d)", error);
                        /* anyway try to go on */
                    }
                    pthread_attr_destroy(&attributs);
                }

                pthread_attr_init(&attributs);
                pthread_attr_setdetachstate(&attributs, PTHREAD_CREATE_DETACHED);
                error = pthread_create(&threads[Notifier], NULL, monitor, NULL);
                pthread_attr_destroy(&attributs);
                if (EXIT_SUCCESS == error) {
                    transfertMgr(NULL);
                } else {
                    CRIT_MSG("pthread_create Notifier error (%d)", error);
                }
#ifdef CORBA_INTERFACE
            } else { /* (pthread_create(&threads[CorbaInterface] == error) */
                CRIT_MSG("pthread_create CorbaInterface error (%d)", error);
            }
#endif /* CORBA_INTERFACE */
        } else {
            error = EINVAL;
            CRIT_MSG("destination directory is not set !");
        }
    } else {
        error = EINVAL;
        CRIT_MSG("source directory is not set !");
    }

    NOTICE_MSG(PACKAGE_NAME " (v" PACKAGE_VERSION ") build on the " __DATE__ " " __TIME__ " ended with status %d", error);
    UNREGISTER_DBGFLAGS(debugFlags);

    return error;
}

