#ifndef _MONITOR_INOTIFY_H_
#define _MONITOR_INOTIFY_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "config.h"
#include <pthread.h>

#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#else /* HAVE_SYS_INOTIFY_H */
#include "inotify-nosys.h"
#endif /* HAVE_SYS_INOTIFY_H */

typedef int (*onInotifyEvent_t) (const char *name,unsigned int nameLength,unsigned int event, void *parameter);

int monitorInotify(const char *directory,const unsigned int monitoredEvent,onInotifyEvent_t onEvent,void *onEventParameter);
int stopMonitoring(pthread_t thread);

#ifdef __cplusplus
}
#endif

#endif /* _MONITOR_INOTIFY_H_ */
