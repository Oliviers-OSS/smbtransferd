#include "globals.h"
#include "configuration.h"
#include "iniparser.h"
#include "filesystem.h"

#ifdef HAVE_DBGFLAGS_DBGFLAGS_H
#include <dbgflags-1/goodies.h>
#endif

#define MODULE_FLAG FLAG_CONFIG

Configuration configuration = {
    0 /* flags */
    , 0 /* SMBDebugLevel */
    , { TO_STRING(CONFIGDIR) CONFIGURATION_FILENAME} /* configuration file */
    /*, LIST_HEAD_INIT(configuration.servers)*/ /* servers list */
    ,{""}
    ,250 /* moveServerDelay */
    ,{'\0'} /* source path */
    ,{'\0'} /* destination path */
    , 0 /* current server position */
    ,{0,0} /*currentServerStartOfUse */
    ,{"mygroup"} /* SMB workgroup */
    ,{"smbtransfertd"} /* SMB login name */
    ,{"password"} /* SMB login password */
    #ifdef _GNU_SOURCE
    ,PTHREAD_RWLOCK_INITIALIZER
    #else
    ,PTHREAD_MUTEX_INITIALIZER
    #endif
};

#ifndef HAVE_DBGFLAGS_DBGFLAGS_H
int parseSyslogLevel(const char *param) {
    int error = EXIT_SUCCESS;
    int newSyslogLevel = 0;

    if (isdigit(param[0])) /* 0x... (hexa value) ) 0.. (octal value or zero) 1 2 3 4 5 6 7 8 9 (decimal value)*/ {
        char *endptr = NULL;
        newSyslogLevel = strtoul(param, &endptr, 0);
        if (endptr != param) {
            if (newSyslogLevel > LOG_UPTO(LOG_DEBUG)) {
                error = EINVAL;
                ERROR_MSG("bad syslog value %d", newSyslogLevel);
            }
            DEBUG_VAR(newSyslogLevel, "%d");
        } else {
            error = EINVAL;
        }
    } else if (strcasecmp(param, "debug") == 0) {
        newSyslogLevel = LOG_UPTO(LOG_DEBUG);
        DEBUG_VAR(newSyslogLevel, "%d");
    } else if (strcasecmp(param, "info") == 0) {
        newSyslogLevel = LOG_UPTO(LOG_INFO);
        DEBUG_VAR(newSyslogLevel, "%d");
    } else if (strcasecmp(param, "notice") == 0) {
        newSyslogLevel = LOG_UPTO(LOG_NOTICE);
        DEBUG_VAR(newSyslogLevel, "%d");
    } else if (strcasecmp(param, "warning") == 0) {
        newSyslogLevel = LOG_UPTO(LOG_WARNING);
        DEBUG_VAR(newSyslogLevel, "%d");
    } else if (strcasecmp(param, "error") == 0) {
        newSyslogLevel = LOG_UPTO(LOG_ERR);
        DEBUG_VAR(newSyslogLevel, "%d");
    } else if (strcasecmp(param, "critical") == 0) {
        newSyslogLevel = LOG_UPTO(LOG_CRIT);
        DEBUG_VAR(newSyslogLevel, "%d");
    } else if (strcasecmp(param, "alert") == 0) {
        newSyslogLevel = LOG_UPTO(LOG_ALERT);
        DEBUG_VAR(newSyslogLevel, "%d");
    }  else if (strcasecmp(param, "not set") == 0) {
        newSyslogLevel = INITIAL_FLAGS_VALUE;
        DEBUG_VAR(newSyslogLevel, "%d");
    }   /*else if (strcasecmp(param,"emergency") == 0) emergency message are not maskable
    {
    newSyslogLevel = LOG_EMERG;
    DEBUG_VAR(newSyslogLevel,"%d");
    }*/
    else {
#ifdef _DEBUG_
        if (isalnum(param[0])) {
            DEBUG_VAR(param, "%s");
        } else {
            DEBUG_VAR(param[0], "%d");
        }
#endif /* _DEBUG_ */
        error = EINVAL;
    }

    SET_LOG_MASK(newSyslogLevel);

    return error;
}
#else
int parseSyslogLevel(const char *param) {
    int error = EXIT_SUCCESS;
    const int newSyslogLevel = stringToSyslogLevel(param);
    if (newSyslogLevel != -1) {
        SET_LOG_MASK(newSyslogLevel);
    } else {
        /* error already printed by the library */
        error = errno;
    }

    return error;
}
#endif /* HAVE_DBGFLAGS_DBGFLAGS_H */

int readConfiguration(void) {
    static unsigned int resumeMode = 0;
    int error = configurationGetWriteAccess(&configuration);
    if (0 == error) {
        dictionary *dict = iniparser_load(configuration.filename);
        if (dict) {
            unsigned int n = 0;
            unsigned int iter = 0;
            char *key = NULL;
            char *value = NULL;
            const char *logLevel = NULL;

            /* read the log level first to be able to activate the following traces
             using the configuration file */
            logLevel = iniparser_getstring(dict, "debug:level", INITIAL_LOG_LEVEL);
            error = parseSyslogLevel(logLevel);
            NOTICE_MSG("log level = %s", logLevel);

            debugFlags.mask = iniparser_getuint(dict,"debug:mask",INITIAL_FLAGS_VALUE);
            NOTICE_MSG("debug flags mask = 0x%X",debugFlags.mask);

            configuration.SMBDebugLevel = iniparser_getint(dict, "debug:libsmb_trace_level", 0);
            NOTICE_MSG("lib SMB trace level = %d", configuration.SMBDebugLevel);

            configuration.flags = 0;

            if (iniparser_getboolean(dict, "daemon:start_suspended", 0)) {
                configuration.flags |= SUSPEND_ON_STARTUP;
                NOTICE_MSG("start suspended");
            }

            if (iniparser_getboolean(dict, "daemon:auto_change_server", 1)) {
                configuration.flags |= CHANGE_SERVER_AUTO_MODE;
                NOTICE_MSG("auto change server set");
            } else {
                NOTICE_MSG("auto change server disabled");
            }

            configuration.moveServerDelay = iniparser_getuint(dict,"daemon:auto_change_delay",INITIAL_FLAGS_VALUE);
            NOTICE_MSG("auto_change_delay = %u ms",configuration.moveServerDelay);

            if (iniparser_getboolean(dict, "daemon:remove_after_transfert", 1)) {
                configuration.flags |= REMOVE_AFTER_TRANSFERT_MODE;
                NOTICE_MSG("remove files after transfert set");
            }

            if (iniparser_getboolean(dict, "daemon:resume_on_startup", 1)) {
                configuration.flags |= RESUME_ON_STARTUP;
                NOTICE_MSG("remove files after transfert set");
            }

            strcpy(configuration.sourceDirectoryName, iniparser_getstring(dict, "paths:local", "/tmp/"PACKAGE));
            NOTICE_MSG("local source path = %s", configuration.sourceDirectoryName);
            error = createDirectory(configuration.sourceDirectoryName,1);
            if ((error != EXIT_SUCCESS) && (error != EEXIST)){
                CRIT_MSG("local source path %s can't be created (%d)",configuration.sourceDirectoryName,error);
            }

            strcpy(configuration.destinationDirectoryName, iniparser_getstring(dict, "paths:remote", "/oc/essai"));
            NOTICE_MSG("remote destination path = %s", configuration.destinationDirectoryName);

            strcpy(configuration.SMBUsername, iniparser_getstring(dict, "login:username", "smbtransferd"));
            NOTICE_MSG("SMB Username = %s", configuration.SMBUsername);

            strcpy(configuration.SMBPassword, iniparser_getstring(dict, "login:password", "smbtransferd"));
            /*NOTICE_MSG("SMBPWD = %s",configuration.SMBPassword);*/

            strcpy(configuration.SMBWorkgroup, iniparser_getstring(dict, "login:workgroup", "mygroup"));
            NOTICE_MSG("SMB Workgroup = %s", configuration.SMBWorkgroup);            

            memset(configuration.servers, 0, sizeof (configuration.servers));
            error = iniparser_findfirst(dict, "servers", &key, &value, &iter);
            if (EXIT_SUCCESS == error) {
                while ((EXIT_SUCCESS == error) && (n < NUMBER_OF_SERVERS_MAX)) {
                    strncpy(configuration.servers[n], value, SERVER_NAME_MAX - 1);
                    configuration.servers[n][SERVER_NAME_MAX - 1] = '\0';
                    NOTICE_MSG("(%d) %s = %s ", n, key, configuration.servers[n]);
                    n++;
                    error = iniparser_findnext(dict, "servers", &key, &value, &iter);
                }
                error = EXIT_SUCCESS;
                if (gettimeofday(&configuration.currentServerStartOfUse,NULL) != 0)
                {
                    error = errno;
                    ERROR_MSG("gettimeofday error %d (%m)",error);
                }
                configuration.currentServerPosition = 0;
            } else {
                CRIT_MSG("no server defined in the servers section of the configuration file %s", configuration.filename);
            }            

            iniparser_freedict(dict);
            dict = NULL;
        } else {
            error = errno; /* error already logged */            
        }
        configurationReleaseWriteAccess(&configuration); /* error already logged */

        if ((ENOENT == error) && (0 == resumeMode)) { /* configuration file not found */
            resumeMode = 1;
            INFO_MSG("configuration file %s not found, trying to read /etc" CONFIGURATION_FILENAME " configuration file instead",configuration.filename);
            strcpy(configuration.filename,"/etc" CONFIGURATION_FILENAME);
            error = readConfiguration();
        }
    } else {
        ERROR_MSG("pthread_rwlock_wrlock error (%d)", error);
    }

    return error;
}

