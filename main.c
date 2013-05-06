#include "globals.h"
#include <getopt.h>
#include <stdio.h>
#include <string.h>

#define MODULE_FLAG FLAG_CONFIG

/*
#undef LOGGER
#undef LOG_OPTS
#define LOGGER consoleLogger
#define LOG_OPTS 0
*/

static __inline void printVersion(void)
{
    printf(PACKAGE_NAME " v" PACKAGE_VERSION "\n");
}

static const struct option longopts[] =
{
    {"console",no_argument, NULL,'c'},
    {"log-level",required_argument, NULL,'l'},
    {"log-mask",required_argument, NULL,'m'},
    {"SMBDebugLevel",required_argument, NULL,'s'},
    {"config-file",required_argument, NULL,'f'},
    { "help"      ,no_argument      , NULL, 'h' },
    { "version"   ,no_argument      , NULL, 'v' },
    { NULL, 0, NULL, 0 }
};

static __inline void printHelp(const char *errorMsg)
{
#define USAGE  "Usage: " PACKAGE_NAME " [OPTIONS] \n" \
    "  -c, --console                     \n"\
    "  -l, --log-level=<level>           \n"\
    "  -l, --log-mask=<mask>             \n"\
    "  -s, --SMBDebugLevel=<level>       \n"\
    "  -f, --config-file=<file's name>   \n"\
    "  -h, --help                        \n"\
    "  -v, --version                     \n"

    if (errorMsg != NULL)
    {
        fprintf(stderr,"Error: %s" USAGE,errorMsg);
    }
    else
    {
        fprintf(stdout,USAGE);
    }

#undef USAGE
}

#define FLAGS_SET           (1<<0)
#define LOG_LEVEL_SET      (1<<1)
#define LOG_MASK_SET       (1<<2)
#define SMB_DBGLEVEL_SET  (1<<3)
typedef struct cmdLineParameters_ {
    unsigned int parametersSet;
    unsigned int flags;
    int logLevel;
    unsigned int logMask;
    int SMBDebugLevel;
} cmdLineParameters;

static inline void setCmdLineParametersFlags(cmdLineParameters  *parameters,const unsigned int flags) {
    parameters->flags |= flags;
    parameters->parametersSet |= FLAGS_SET;
    DEBUG_VAR(parameters->flags,"0x%X");
}

static inline void setCmdLineParametersLogLevel(cmdLineParameters  *parameters,const int level) {
    parameters->logLevel = level;
    parameters->parametersSet |= LOG_LEVEL_SET;
    DEBUG_VAR(parameters->logLevel,"%d");
}

static inline void setCmdLineParametersLogMask(cmdLineParameters  *parameters,const unsigned int mask) {
    parameters->logMask = mask;
    parameters->parametersSet |= LOG_MASK_SET;
    DEBUG_VAR(parameters->logMask,"0x%X");
}

static inline void setCmdLineParametersSMBDebugLevel(cmdLineParameters  *parameters,int level) {
    parameters->SMBDebugLevel = level;
    parameters->parametersSet |= SMB_DBGLEVEL_SET;
    DEBUG_VAR(parameters->SMBDebugLevel,"%d");
}

static inline void applyCmdLineParameters(cmdLineParameters  *parameters,Configuration *configuration) {
    if (parameters->parametersSet & LOG_LEVEL_SET) {
        SET_LOG_MASK(parameters->logLevel);
    }

    if (parameters->parametersSet & LOG_MASK_SET) {
        debugFlags.mask = parameters->logMask;
    }

    if (parameters->parametersSet & SMB_DBGLEVEL_SET) {
        configuration->SMBDebugLevel = parameters->SMBDebugLevel;
    }

    if (parameters->parametersSet & FLAGS_SET) {
        configuration->flags |= parameters->flags;
    }
}

static inline int parseCmdLine(int argc, char *argv[],cmdLineParameters  *parameters) {
    int error = EXIT_SUCCESS;
    int optc;

    memset(parameters,0,sizeof(cmdLineParameters));
    error = configurationGetWriteAccess(&configuration);
    if (0 == error) {
        while (((optc = getopt_long (argc, argv, "l:m:s:f:chv", longopts, NULL)) != -1) && (EXIT_SUCCESS == error))
        {
            char errorMsg[100];
            switch (optc)
            {
            case 'l':
                error = parseSyslogLevel(optarg);
                if (EXIT_SUCCESS == error) {
                    setCmdLineParametersLogLevel(parameters,SET_LOG_MASK(0));
                }
                break;
            case 'm':
                setCmdLineParametersLogMask(parameters,strtol(optarg,NULL,0));
                break;
            case 's':
                setCmdLineParametersSMBDebugLevel(parameters,atoi(optarg));
               break;
            case 'f':
               setConfigurationFile(optarg);
               DEBUG_VAR(configuration.filename,"%s");
               break;
            case 'c':
               setCmdLineParametersFlags(parameters,CONSOLE_MODE);
               setConsoleMode(configuration);
               DEBUG_MSG("consoleMode set");
               break;
            case 'h':
                printHelp(NULL);
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                printVersion();
                exit(EXIT_SUCCESS);
                break;
            case '?':
                /* getopt_long} already printed an error message.  */
                error = EINVAL;
                printHelp("");
                break;                        
            default:
                printHelp("invalid parameter");
                error = EINVAL;
                break;
            } /* switch (optc) */
        } /* while ((optc = getopt_long (argc, argv, " */
        configurationReleaseWriteAccess(&configuration);
    } /* error already logged */
    return error;
}

#ifdef CORBA_INTERFACE

static inline int removeORBParameters(int argc, char *argv[],char *parameters[]) {
    int n = 0;
    int i = 0;

    while(i<argc) {
        if (strncasecmp(argv[i],"-ORB",4) != 0) {
            parameters[n++] = argv[i++];
        } else {
            // ORB's parameter, check if the next one is its argument
            ++i;
            if (argv[i][0] != '-') {
                ++i;
            }
        }
    }
    return n;
}

static inline int setConfiguration(int argc, char *argv[]) {
    int error = EXIT_SUCCESS;
    cmdLineParameters  parameters;
    char **SMBTransferdParameters = calloc(argc,sizeof(char*));

    if (SMBTransferdParameters != NULL) {
        const int newArgC = removeORBParameters(argc,argv,SMBTransferdParameters);
        error = parseCmdLine(newArgC,SMBTransferdParameters,&parameters);
        free(SMBTransferdParameters);
        SMBTransferdParameters = NULL;
        if (EXIT_SUCCESS == error) {
            error = readConfiguration();
            if (EXIT_SUCCESS == error) {
                applyCmdLineParameters(&parameters,&configuration);
            }
        }        
    } else {
        error = ENOMEM;
        ERROR_MSG("failed to allocate %d bytes for SMBTransferdParameters",sizeof(char*)*argc);
    }
    return error;
}
#else /* CORBA_INTERFACE */

static inline int setConfiguration(int argc, char *argv[]) {
    int error = EXIT_SUCCESS;
    cmdLineParameters  parameters;
    char **SMBTransferdParameters = calloc(argc,sizeof(char*));

    if (SMBTransferdParameters != NULL) {
        error = parseCmdLine(argc,argv,&parameters);
        if (EXIT_SUCCESS == error) {
            error = readConfiguration();
            if (EXIT_SUCCESS == error) {
                applyCmdLineParameters(&parameters,&configuration);
            }
        }
        free(SMBTransferdParameters);
        SMBTransferdParameters = NULL;
    } else {
        error = ENOMEM;
        ERROR_MSG("failed to allocate %d bytes for SMBTransferdParameters",sizeof(char*)*argc);
    }
    return error;
}

#endif /* CORBA_INTERFACE */

int main(int argc, char** argv) {
    int error = setConfiguration(argc,argv);
    if (EXIT_SUCCESS == error) {
	pid_t daemonProcessId = 0;

	if (!isConsoleModeSet(configuration)) {
           error = deamonize(sigHupSignalHandler,sigTermSignalHandler,&daemonProcessId);
        } else {
            /* minimal initialization */
            /* init logger */
            openlogex(identity, LOG_PID | LOG_TID, LOG_DAEMON);
        }

        if ((EXIT_SUCCESS == error) && (0 == daemonProcessId)) { /* daemonProcessId != 0 means the running process is the parent */
           DEBUG_VAR(daemonProcessId,"%d");
           error = run(argc,argv);
        }
    }
    return error;
}
