#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

#include <linux/limits.h>
#include <string.h>
#include <pthread.h>
#include "config.h"
#include "debug.h"
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define CONFIGURATION_FILENAME "/" PACKAGE ".conf"

#define MODULE_FLAG FLAG_CONFIG

#if 0
#include "klist.h"

typedef struct serverList_ {
  struct list_head list;
  char name[20];
  /*char address[16];*/
} serverList;
#endif

#define SERVER_NAME_MAX        32
#define NUMBER_OF_SERVERS_MAX 12

#define CONSOLE_MODE                         (1<<0)
#define REMOVE_AFTER_TRANSFERT_MODE     (1<<1)
#define CHANGE_SERVER_AUTO_MODE          (1<<2)
#define SUSPEND_ON_STARTUP                 (1<<3)
#define RESUME_ON_STARTUP                  (1<<4)

typedef struct Configuration_ {
   unsigned int flags;
   int SMBDebugLevel;
   char filename[PATH_MAX];
   /*list_t servers;*/
   char servers[NUMBER_OF_SERVERS_MAX][SERVER_NAME_MAX];
   unsigned int moveServerDelay;
   char sourceDirectoryName[PATH_MAX];
   char destinationDirectoryName[PATH_MAX];
   unsigned int currentServerPosition;
   struct timeval currentServerStartOfUse;
   char SMBWorkgroup[20];
   char SMBUsername[20];
   char SMBPassword[20];
#ifdef _GNU_SOURCE
    pthread_rwlock_t rwlock;
#else
    pthread_mutex_t mutex;
#endif
} Configuration;

extern Configuration configuration;

#ifdef __cplusplus
}
#endif

/* 
 * access lock MUST BE SET to use the following observers and mutators
 * or you must be sure that this value can't change
 */
#define setConsoleMode(x)                  (x.flags |= CONSOLE_MODE)
#define setRemoveAfterTransfert(x)      (x.flags |= REMOVE_AFTER_TRANSFERT_MODE)

#define isConsoleModeSet(x)               (x.flags & CONSOLE_MODE)
#define isChangeServerAutoModeSet(x)   (x.flags & CHANGE_SERVER_AUTO_MODE)
#define setChangeServerAutoMode(x)      (x.flags |= CHANGE_SERVER_AUTO_MODE)
#define unsetChangeServerAutoMode(x)   (x.flags &= ~CHANGE_SERVER_AUTO_MODE)
#define isRemoveAfterTransfertSet(x)   (x.flags & REMOVE_AFTER_TRANSFERT_MODE)
#define getSourceDirectoryName(x)         x.sourceDirectoryName
#define getDestinationDirectoryName(x)  x.destinationDirectoryName
#define getSMBWorkgroup(x)                  x.SMBWorkgroup
#define getSMBUsername(x)                   x.SMBUsername
#define getSMBPassword(x)                   x.SMBPassword
#define isResumeOnStartupSet(x)          (x.flags & RESUME_ON_STARTUP)
#define isSuspendOnStartupSet(x)           (x.flags & SUSPEND_ON_STARTUP)

static inline void setConfigurationFile(const char *filename) {
    strcpy(configuration.filename,filename);
}

/* 
 * lock management functions
 */

#ifdef _GNU_SOURCE
static inline int configurationGetReadAccess(Configuration *configuration) {
    int error = pthread_rwlock_rdlock(&configuration->rwlock);
    if (error != 0) {
        ERROR_MSG("pthread_rwlock_rdlock error (%d)",error);
    }
    return error;
}

static inline int configurationReleaseReadAccess(Configuration *configuration) {
    int error = pthread_rwlock_unlock(&configuration->rwlock);
    if (error != 0) {
        ERROR_MSG("pthread_rwlock_unlock error (%d)",error);
    }
    return error;
}

static inline int configurationGetWriteAccess(Configuration *configuration) {
    int error = pthread_rwlock_wrlock(&configuration->rwlock);
    if (error != 0) {
        ERROR_MSG("pthread_rwlock_wrlock error (%d)",error);
    }
    return error;
}

static inline int configurationReleaseWriteAccess(Configuration *configuration) {
    int error = pthread_rwlock_unlock(&configuration->rwlock);
    if (error != 0) {
        ERROR_MSG("pthread_rwlock_unlock error (%d)",error);
    }
    return error;
}

static inline int configurationFreeLock(Configuration *configuration) {
    int error = pthread_rwlock_destroy(&configuration->rwlock);
    if (error != 0) {
        ERROR_MSG("pthread_rwlock_destroy error (%d)",error);
    }
    return error;
}

#else /* _GNU_SOURCE */

static inline int configurationGetReadAccess(Configuration *Configuration) {
    int error = pthread_mutex_lock(&Configuration->mutex);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_lock error (%d)",error);
    }
    return error;
}

static inline int configurationReleaseReadAccess(Configuration *Configuration) {
    int error = pthread_mutex_unlock(&Configuration->mutex);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_unlock error (%d)",error);
    }
    return error;
}

static inline int configurationGetWriteAccess(Configuration *Configuration) {
    int error = pthread_mutex_lock(&Configuration->mutex);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_lock error (%d)",error);
    }
    return error;
}

static inline int configurationReleaseWriteAccess(Configuration *Configuration) {
    int error = pthread_mutex_unlock(&Configuration->mutex);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_unlock error (%d)",error);
    }
    return error;
}

static inline int configurationFreeLock(Configuration *Configuration) {
    int error = pthread_mutex_destroy(&Configuration->mutex);
    if (error != 0) {
        ERROR_MSG("pthread_mutex_destroy error (%d)",error);
    }
    return error;
}
#endif /* _GNU_SOURCE */

/* 
 * threads safe versions
 */
static inline const char* getCurrentServer(Configuration *configuration,struct timeval *currentServerStartOfUse) {
    const char *currentServer = "";
    int error = configurationGetReadAccess(configuration);
    if (0 == error) {
        if (currentServerStartOfUse != NULL) {
            currentServerStartOfUse->tv_sec = configuration->currentServerStartOfUse.tv_sec;
            currentServerStartOfUse->tv_usec = configuration->currentServerStartOfUse.tv_usec;
        }
        currentServer = configuration->servers[configuration->currentServerPosition];
        error = configurationReleaseWriteAccess(configuration);
    }
    return currentServer;
}

static inline int changeCurrentServer(Configuration *configuration,const struct timeval *previousServerStartOfUse) {
    int error = configurationGetWriteAccess(configuration);
    if (0 == error) {
            /* currentServer already changed (since last getCurrentServer call) ? */
        if (   (configuration->currentServerStartOfUse.tv_sec == previousServerStartOfUse->tv_sec)
            && (configuration->currentServerStartOfUse.tv_usec == previousServerStartOfUse->tv_usec)) {
       
            if (gettimeofday(&configuration->currentServerStartOfUse,NULL) == 0)
            {
                configuration->currentServerPosition++;
                if (configuration->currentServerPosition >= NUMBER_OF_SERVERS_MAX) {
                    configuration->currentServerPosition = 0;
                } else {
                    if ('\0' == configuration->servers[configuration->currentServerPosition][0]) {
                        configuration->currentServerPosition = 0;
                    }
                }
                DEBUG_VAR(configuration->currentServerPosition,"%d");
            } else {
                error = errno;
                ERROR_MSG("gettimeofday error %d (%m)",error);
            }
        } else {
            DEBUG_MSG("current server has already been changed ({%d,%d} > {%d,%d})",configuration->currentServerStartOfUse.tv_sec,configuration->currentServerStartOfUse.tv_usec,previousServerStartOfUse->tv_sec,previousServerStartOfUse->tv_usec);
        }
        error = configurationReleaseWriteAccess(configuration); /* error already logged */
    } /* configurationGetWriteAccess (error already logged) */
        
    return error;
}

static inline int getServerPosition(Configuration *configuration, const char *newServer) {
    int position = 0;
    while ( (strcmp(configuration->servers[position],newServer) != 0) && (position < NUMBER_OF_SERVERS_MAX)) {
        position++;
    }
    return position;
}

static inline int setCurrentServer(Configuration *configuration, const char *newServer) {
    int error = EXIT_SUCCESS;
    const int serverPosition = getServerPosition(configuration,newServer);
    if (serverPosition < NUMBER_OF_SERVERS_MAX) {
        struct timeval currentServerStartOfUse;
        if (gettimeofday(&currentServerStartOfUse,NULL) == 0) {
            error = configurationGetWriteAccess(configuration);
            if (0 == error) {
                configuration->currentServerPosition = serverPosition;
                DEBUG_VAR(configuration->currentServerPosition,"%d");
                
                error = configurationReleaseWriteAccess(configuration); /* error already logged */
            } /* configurationGetWriteAccess (error already logged) */
       } else {
          error = errno;
          ERROR_MSG("gettimeofday error %d (%m)",error);
       }
    } else { /* !(serverPosition < NUMBER_OF_SERVERS_MAX) */
        ERROR_MSG("server %s not found",newServer);
        error = ENOENT;
    }
    return error;
}

#ifdef __cplusplus
extern "C"
{
#endif

/* misc */
int parseSyslogLevel(const char *param);

#undef MODULE_FLAG

#ifdef __cplusplus
}
#endif

#endif /* _CONFIGURATION_H_ */
