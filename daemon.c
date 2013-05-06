#include "globals.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <linux/limits.h>
#include <signal.h>

#define MODULE_FLAG FLAG_DAEMON
#define LOCK_FILE_NAME  "/var/log/" identity ".pid"

static inline void closeAllFileDescriptors()
{
    int fd = 0;
    for (fd = getdtablesize() - 1; fd >= 0; fd--) {
        close(fd);
    }
}

static inline int setStdFileDescriptorsToDevNull()
{
    int error = EXIT_SUCCESS;
    int newStdin = open("/dev/null", O_RDWR); /* stdin */
    if (newStdin >= 0) {
        /* fd must be 0 (stdin) */
        int newStdout = dup(newStdin);
        if (newStdout > 0) {
            int newStderr = dup(newStdin);
            if (newStderr < 0) {
                error = errno;
                ERROR_MSG("dup /dev/null read & write for stderr error %d (%m)", error);
            }
        } else {
            error = errno;
            ERROR_MSG("dup /dev/null read & write for stdout error %d (%m)", error);
        }
    } else {
        error = errno;
        ERROR_MSG("open /dev/null read & write for stdin error %d (%m)", error);
    }
    return error;
}

static inline int unsetLock()
{
    int error = EXIT_SUCCESS;
    if (unlink(LOCK_FILE_NAME) == -1) {
        error = errno;
        ERROR_MSG("unlink " LOCK_FILE_NAME " error %d (%m)", error);
    } else {
        DEBUG_MSG("lock file " LOCK_FILE_NAME "deleted");
    }
    return error;
}

static void freeRessources(void)
{
    unsetLock();
}

static inline int daemonInit(signalhandler_t sigHupSignalHandler, signalhandler_t sigTermSignalHandler)
{
    int error = EXIT_SUCCESS;
    pid_t sessionId = -1;

    /* change the file mode mask */
    //umask(0);

    /* init logger */
    openlogex(identity, LOG_PID | LOG_TID, LOG_DAEMON);

    /* create a new session ID for the child process */
    sessionId = setsid();
    if (sessionId > 0) {
        DEBUG_MSG("sessionID = %u", sessionId);
        /* change the current working directory to allow any file operations like umount */
        if (chdir("/") < 0) {
            error = errno;
            ERROR_MSG("chdir to / error %d (%m)", error);
        }

        closeAllFileDescriptors();
        setStdFileDescriptorsToDevNull(); /* for security reasons */

        if (signal(SIGHUP, sigHupSignalHandler) == SIG_ERR) {
            ERROR_MSG("error setting signal SIGHUP handler");
            error = EINVAL;
        }

        if (signal(SIGTERM, sigTermSignalHandler) == SIG_ERR) {
            ERROR_MSG("error setting signal SIGTERM handler");
            error = EINVAL;
        }
    } else {
        error = errno;
        CRIT_MSG("setsid error %d (%m)", error);
    }
    return error;
}

/*
#undef LOGGER
#undef LOG_OPTS
#define LOGGER consoleLogger
#define LOG_OPTS 0
*/

static inline int getFullProcessName(const pid_t pid, char *processName, size_t processNameAllocatedSize)
{
    int error = EXIT_SUCCESS;
    char procEntryName[PATH_MAX];
    ssize_t n = 0;

    sprintf(procEntryName, "/proc/%u/exe", pid);
    n = readlink(procEntryName, processName, processNameAllocatedSize);
    if (n != -1) {
        processName[n] = '\0'; /* because readlink does not append a  NULL  character to  buf */
        DEBUG_VAR(processName, "%s");
    } else {
        error = errno;
        ERROR_MSG("readlink %s error %d (%m)", procEntryName, error);
    }

    return error;
}

static inline int getCurrentFullProcessName(char *processName, size_t processNameAllocatedSize)
{
    const pid_t pid = getpid();
    return getFullProcessName(pid, processName, processNameAllocatedSize);
}

static inline int sameProcessAlreadyRunning(const pid_t previousPID)
{
    int error = EXIT_SUCCESS;
    char runningProcessName[PATH_MAX];
    error = getFullProcessName(previousPID, runningProcessName, sizeof(runningProcessName));
    if (EXIT_SUCCESS == error) {
        /* this is a PID of a running process: check its name against the current */
        char currentProcessName[PATH_MAX];
        error = getCurrentFullProcessName(currentProcessName, sizeof(currentProcessName));
        if (EXIT_SUCCESS == error) {
            DEBUG_VAR(currentProcessName, "%s");
            DEBUG_VAR(runningProcessName, "%s");
            if (strcmp(currentProcessName, runningProcessName) == 0) {
                /* same process */
                error = EEXIST;
                ERROR_MSG("another instance of %s is still running (%u)", runningProcessName, previousPID);
            } else {
                /* not the same name */
                DEBUG_MSG("the PID %u is now %s and no more %s", previousPID, runningProcessName, currentProcessName);
                error = EXIT_SUCCESS;
            }
        } else {
            /* getCurrentFullProcessName error already logged */
            /*error = EXIT_SUCCESS;*/
        }
    } else {
        /* error: this PID is no more a running process ? (error details already logged) */
        error = EXIT_SUCCESS;
    }
    return error;
}

static inline pid_t stringPIDToPID(const char *string)
{
    const char *cursor = string;
    pid_t pid = 0;
    while ((*cursor >= '0') && (*cursor <= '9')) {
        pid = pid * 10 + (*cursor) - '0';
        cursor++;
    }
    return pid;
}

static inline int setLock()
{
    int error = EXIT_SUCCESS;
    int fd = open(LOCK_FILE_NAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRWXG | S_IROTH);
    if (fd > 0) {
        /* check if another instance is running */
        /* first get an exclusiv access to this file (during this function) */
        int n = lockf(fd, F_TLOCK, 0);
        if (n != -1) {
            char fileContent[15]; /* because (unsigned int)-1 needs 11 characters to be printed */
            n = read(fd, &fileContent, sizeof(fileContent));
            if (n > 0) {
                /* still running ? */
                pid_t previousPID = stringPIDToPID(fileContent);
                if (previousPID != 0) {
                    error = sameProcessAlreadyRunning(previousPID);
                } else {
                    DEBUG_MSG("the content of the file " LOCK_FILE_NAME " is not valid (%s)", fileContent);
                }
            } else { /* read(fd,&fileContent,sizeof(fileContent)) <= 0 */
                if (-1 == n) {
                    /* not an error */
                    DEBUG_MSG("read " LOCK_FILE_NAME " error (%d)", errno);
                } else {
                    DEBUG_MSG("read " LOCK_FILE_NAME " return only %d bytes", n);
                }
            }

            if (error != EEXIST) {
                const pid_t pid = getpid();
                ssize_t written = 0;
                if (ftruncate(fd, 0) == -1) {
                    error = errno;
                    WARNING_MSG("ftruncate to 0 file " LOCK_FILE_NAME " error %d (%m)", error);
                }
                n = sprintf(fileContent, "%u", pid);
                written = write(fd, fileContent, n);
                if (written != n) {
                    if (-1 == written) {
                        error = errno;
                        ERROR_MSG("write into " LOCK_FILE_NAME " error %d (%m)", error);
                    } else {
                        error = EIO;
                        ERROR_MSG("only %d bytes have been written into " LOCK_FILE_NAME, written);
                    }
                } /* (written != n) */
            } /* (error != EEXIST) error already logged */

            /* unlock this file */
            n = lockf(fd, F_ULOCK, 0);
            if (-1 == n) {
                if (EXIT_SUCCESS == error) { /* don't overwrite a previous error */
                    error = errno;
                }
                ERROR_MSG("flock F_ULOCK " LOCK_FILE_NAME " error %d (%m)", errno);
            }
        } else {
            /* some one else may have lock it: another process is running this function ? */
            error = errno;
            ERROR_MSG("flock F_TLOCK " LOCK_FILE_NAME " error %d (%m)", error);
        }

        if (close(fd) != 0) {
            error = errno;
            ERROR_MSG("close " LOCK_FILE_NAME " error %d (%m)", error);
        }
    } else {
        error = errno;
        ERROR_MSG("open " LOCK_FILE_NAME " error %d (%m)", error);
    }
    return error;
}

int deamonize(signalhandler_t sigHupSignalHandler, signalhandler_t sigTermSignalHandler, pid_t *daemonProcessId)
{
    int error = EXIT_SUCCESS;

    /* fork off the parent process */
    *daemonProcessId = fork();
    if (0 == *daemonProcessId) {
        /* child process */
        error = setLock();
        if (error != EEXIST) {
            /* not a fatal error */
            if (atexit(freeRessources) != 0) {
                ERROR_MSG("atexit error");
            }
            error = daemonInit(sigHupSignalHandler, sigTermSignalHandler);
        } /* error already logged */
    } else if (*daemonProcessId > 0) {
        /* parent process */
        //NOTICE_MSG("daemon started with PID %u", *daemonProcessId);
    } else {
        error = errno;
        CRIT_MSG("fork error %d (%m)", error);
    }

    return error;
}
