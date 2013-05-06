/* 
 * File:   globals.h
 * Author: oc
 *
 * Created on July 10, 2010, 7:16 PM
 */

#ifndef _GLOBALS_H_
#define	_GLOBALS_H_

#define identity  "smbtransferd"
#include "config.h"

#ifndef PACKAGE_NAME
#define PACKAGE_NAME identity
#endif /* PACKAGE_NAME */

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "2.0"
#endif /* PACKAGE_VERSION */

#ifndef THREADS_POOL_SIZE
#define THREADS_POOL_SIZE 5
#endif /* THREADS_POOL_SIZE */

#ifndef CONFIGDIR
#define CONFIGDIR /etc
#endif /* CONFIGDIR */

#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#ifndef offsetof /* seems missing on RHEL5 and defined in linux/stddef.h on others for examples */
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif /* offsetof */

#include "configuration.h"
#include "runtime.h"
#include "debug.h"
#include "daemon.h"
#include "monitorInotify.h"
#include "smbcopy.h"
#include "remoteServersSetEventSubscriberMgr.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(_SYS_SYSLOG_EX_H)
    #define SET_LOG_MASK(x) setlogmaskex(x)
#else
    #define SET_LOG_MASK(x) setlogmask(x)
#endif

#ifdef _DEBUGFLAGS_H_
#define REGISTER_DBGFLAGS(x)    registerDebugFlags(&x)
#define UNREGISTER_DBGFLAGS(x)  unregisterDebugFlags(&x)
#else
#define REGISTER_DBGFLAGS(x)
#define UNREGISTER_DBGFLAGS(x)
#endif /*_DEBUGFLAGS_H_*/

#ifdef DISABLE_CORBA_NS_KIND
#define CORBA_NS_KIND ""
#else /* DISABLE_CORBA_NS_KIND */
#define CORBA_NS_KIND "remoteServersSet"
#endif /* DISABLE_CORBA_NS_KIND */

extern unsigned int global_command;
#define CMD_NOTHING 0
#define CMD_STOP    1

extern Configuration configuration;

#ifdef __cplusplus
}
#endif

#endif	/* _GLOBALS_H_ */

