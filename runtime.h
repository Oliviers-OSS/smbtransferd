
#ifndef _RUNTIME_H_
#define	_RUNTIME_H_

#ifdef	__cplusplus
extern "C" {
#endif

void sigHupSignalHandler(int receivedSignal);
void sigTermSignalHandler(int receivedSignal);
int run(int argc, char** argv);
int resumeDaemonStartup();

#ifdef	__cplusplus
}
#endif

#endif	/* _RUNTIME_H_ */

