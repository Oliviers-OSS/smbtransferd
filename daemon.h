#ifndef _DAEMON_H_
#define _DAEMON_H_

#ifdef __cplusplus
extern "C"
{
#endif

//#include <signal.h>
#include <sys/types.h>

typedef void (*signalhandler_t)(int);

int deamonize(signalhandler_t sigHupHandler, signalhandler_t sigTermHandler,pid_t *daemonProcessId);

#ifdef __cplusplus
}
#endif

#endif /* _DAEMON_H_ */
