/* 
 * File:   messagesQueue.h
 * Author: oc
 *
 * Created on July 17, 2010, 4:38 PM
 */

#ifndef _MESSAGESQUEUE_H_
#define	_MESSAGESQUEUE_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include "config.h"
#include "globals.h"

#define MODULE_FLAG FLAG_RUNTIME

#ifdef HAVE_MQUEUE_H
/* POSIX messagesQueue */
#include <mqueue.h>
    
typedef  mqd_t messageQueueId;

static inline int createMessagesQueue(const char *name,int mode,messageQueueId *queue) {
    int error = EXIT_SUCCESS;
    /*struct mq_attr attributs;

    memset(&attributs,0,sizeof(attributs));
    attributs.mq_maxmsg = 10;
    attributs.mq_msgsize = maxMsgSize;*/
    *queue = mq_open(name,mode|O_CREAT,S_IRUSR|S_IWUSR,NULL);
    if ((mqd_t) -1 == *queue) {
        error = errno;
        ERROR_MSG("mq_open %s %s error %d (%m)",name,(mode == O_RDONLY)?"O_RDONLY":(mode == O_WRONLY)?"O_WRONLY":(mode == O_RDWR)?"O_RDWR":"BAD MODE PARAMETER",error);
    } else {
        #ifdef _DEBUG_
        //struct mq_attr attributs;
        /*if (mq_getattr(*queue,&attributs) == 0) {
            DEBUG_VAR(attributs.mq_maxmsg,"%d");
            DEBUG_VAR(attributs.mq_curmsgs,"%d");
            DEBUG_VAR(attributs.mq_msgsize,"%d");
        } else {
            error = errno;
            ERROR_MSG("mq_getattr %s error %d (%m)",name,error);
        }*/
        #endif
    }
    return error;
}

static inline int closeMessagesQueue(messageQueueId queue) {
    int error = EXIT_SUCCESS;
    if (mq_close(queue) != 0) {
        error = errno;
        ERROR_MSG("mq_close error %d (%m)",error);
    }
    return error;
}

static inline int deleteMessagesQueue(const char *name) {
    int error = EXIT_SUCCESS;
    if (mq_unlink(name) != 0) {
        error = errno;
        ERROR_MSG("mq_unlink %s error %d (%m)",name,error);
    }
    return error;
}

static inline int writeToMessagesQueue(messageQueueId queue,const char *message,size_t messageLength, unsigned int messagePriority) {
    int error = EXIT_SUCCESS;
    if (mq_send(queue,message,messageLength,messagePriority) != 0) {
        error = errno;
        ERROR_MSG("mq_send error %d (%m)",error);
    }
    return error;
}

static inline int readFromMessagesQueue(messageQueueId queue,char *message,size_t *messageLength, unsigned int *messagePriority ) {
    int error = EXIT_SUCCESS;
    const mqd_t n =  mq_receive(queue,message,*messageLength,messagePriority);
    DEBUG_VAR(n,"%d");
    if (n != 0){
        *messageLength = n;
    } else {
        error = errno;
        ERROR_MSG("mq_receive error %d (%m)",error);
    }
    return error;
}

#else 
/* System V messagesQueue when POSIX ones are not available */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

typedef  int messageQueueId;
    
#endif /* HAVE_MQUEUE_H */


#undef  MODULE_FLAG

#ifdef	__cplusplus
}
#endif

#endif	/* _MESSAGESQUEUE_H_ */

