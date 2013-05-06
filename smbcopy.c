#include "globals.h"
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <libsmbclient.h>
#include <stdlib.h>
#include <dirent.h>
#include "filesystem.h"


#define MODULE_FLAG         FLAG_SAMBA
#define BUFFER_SIZE         4096

#ifndef SMBC_DEBUG_LEVEL
#define SMBC_DEBUG_LEVEL     2 /*9*/
#endif /* SMBC_DEBUG_LEVEL*/

static void getAuthentificationData(const char * pServer,
                                      const char * pShare,
                                      char * pWorkgroup,
                                      int maxLenWorkgroup,
                                      char * pUsername,
                                      int maxLenUsername,
                                      char * pPassword,
                                      int maxLenPassword) {
    int error = configurationGetWriteAccess(&configuration); /* error already logged */
    if (0 == error) {
        strncpy(pWorkgroup,getSMBWorkgroup(configuration),maxLenWorkgroup);
        strncpy(pUsername,getSMBUsername(configuration),maxLenUsername);
        strncpy(pPassword,getSMBPassword(configuration),maxLenPassword);
        pWorkgroup[maxLenWorkgroup-1] = pUsername[maxLenUsername-1] = pPassword[maxLenPassword-1] = '\0';
        DEBUG_MSG("sending credentials...");
        error  = configurationReleaseWriteAccess(&configuration); /* error already logged */
    }
}

static int smbcInit(void) {
    static int smbc_init_error = -1;    
    static pthread_mutex_t smbcInitMutex = PTHREAD_MUTEX_INITIALIZER;
    int error = EXIT_SUCCESS;

    if (smbc_init_error != EXIT_SUCCESS) {
        error = pthread_mutex_lock(&smbcInitMutex);
        if (0 == error) {
            
            /* check again the current status if this thread was suspended and another one have done it during this time */
            if (smbc_init_error != EXIT_SUCCESS) {
                error = smbc_init_error  = smbc_init(getAuthentificationData, SMBC_DEBUG_LEVEL);
                if (smbc_init_error != EXIT_SUCCESS) {
                    ERROR_MSG("smbc_init error %d (%m)",error);
                    publishEvent(eventError,error,"smbc_init error");
                } else {
                    DEBUG_MSG("smbc_init OK");
                }
            }

            error = pthread_mutex_unlock(&smbcInitMutex);
            if (error != 0) {
                ERROR_MSG("pthread_mutex_unlock error %d",error);
            }
        } else {
            ERROR_MSG("pthread_mutex_lock error %d",error);
        }
    }
    return smbc_init_error;
}

static int copyFile(const char *localFileName,const char *urlRemoteFileName) {
    int error = EXIT_SUCCESS;
    int fd = open(localFileName, O_RDONLY);
    if (fd != -1) {
        int smbFd = smbc_open(urlRemoteFileName, O_CREAT | O_WRONLY | O_TRUNC, 0);
        if (smbFd != -1) {
            char buffer[2048];
            ssize_t size_read = 0;
            ssize_t size_written = 0;

            do {
                size_read = read(fd, buffer, sizeof(buffer));
                if (size_read > 0) {
                    size_written = smbc_write(smbFd, buffer, size_read);
                    if (size_written != size_read) {
                        if (-1 == size_written) {
                            error = errno;
                            ERROR_MSG("error writing to file %s %d (%m)", urlRemoteFileName, error);
                        } else {
                            error = EIO;
                            ERROR_MSG("error writing to file %s (only %d/%d written)", urlRemoteFileName, size_written, size_read);
                        }
                        publishEvent(eventError,error,"%s write error", urlRemoteFileName);
                        size_read = 0; /* to abord */
                    }
                } else if (-1 == size_read) {
                    error = errno;
                    ERROR_MSG("error reading from file %s %d (%m)", localFileName, error);
                    publishEvent(eventError,error,"%s read error", localFileName);
                }
            } while ((size_read > 0));

            if (EXIT_SUCCESS == error) {
                INFO_MSG("file %s successfully copied to %s", localFileName, urlRemoteFileName);
                publishEvent(eventInformation,EXIT_SUCCESS,"%s copied", localFileName);
            }

            if (smbc_close(smbFd) < 0) {
                error = errno;
                ERROR_MSG("smbc_close %d %m", error);
            }
        } else {
            error = errno;
            ERROR_MSG("can't open file %s for writing error %d (%m)", urlRemoteFileName, error);
            publishEvent(eventError,error,"%s open error", urlRemoteFileName);
        }

        if (close(fd) == -1) {
            if (EXIT_SUCCESS == error) {
                error = errno;
            }
            ERROR_MSG("close %s error %d (%m)", localFileName, errno);
        }
    } else {
        error = errno;
        ERROR_MSG("can't open %s for reading error %d (%m)", localFileName, error);
        publishEvent(eventError,error,"%s open error", localFileName);
    }
    return error;
}

int smbCopyFile(const char *filename,const char* server, const char* remotePath) {    
    int error = smbcInit();

    if (0 == error) {
        char SMBPath[PATH_MAX];
        const char *correctedRemotePath = remotePath;
        const char *shortFileName = strrchr(filename,'/');
        if ('/' == correctedRemotePath[0]) {
            correctedRemotePath++;
        }
        if (NULL == shortFileName) {
            shortFileName = filename;
        } else {
            shortFileName++;
        }
        sprintf(SMBPath, "smb://%s/%s/%s"IN_PROGRESS_EXTENSION, server, correctedRemotePath, shortFileName);
        DEBUG_VAR(SMBPath, "%s");
        error = copyFile(filename,SMBPath);
        if (EXIT_SUCCESS == error) {
            char newSMBPath[PATH_MAX];
            sprintf(newSMBPath, "smb://%s/%s/%s", server, correctedRemotePath, shortFileName);
            error = smbc_rename(SMBPath, newSMBPath);
            if (error != 0) {
                error = errno;
                ERROR_MSG("rename %s => %s error %d (%m)",SMBPath,newSMBPath,error);
            } else {
                INFO_MSG("%s successfully renamed to %s ",SMBPath,newSMBPath);
            }
        } /* error already logged */
    } else {
        ERROR_MSG("smbc_init %d ", error);
    }
    return error;
}

static int copyDirectory(const char *localDirectoryName, const char *urlRemoteDirectoryName) {
    int error = EXIT_SUCCESS;
    char currentLocalDirectory[PATH_MAX];
    if (getcwd(currentLocalDirectory, sizeof(currentLocalDirectory)) != NULL) {
        /* create remote direcory */
        if (smbc_mkdir(urlRemoteDirectoryName, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
            if (errno != EEXIST) {
                error = errno;
                ERROR_MSG("smbc_mkdir %s error %d (%m)", urlRemoteDirectoryName, error);
            } else {
                DEBUG_MSG("remote directory %s already exists", urlRemoteDirectoryName);
            }
        } else {
            DEBUG_MSG("remote directory %s successfully created", urlRemoteDirectoryName);
        }

        if (EXIT_SUCCESS == error) {
            DIR *directory = opendir(localDirectoryName);
            if (directory != NULL) {
                if (chdir(localDirectoryName) == 0) {
                    struct dirent *directoryEntry = NULL;
                    errno = EXIT_SUCCESS;
                    do {
                        directoryEntry = readdir(directory);
                        if (directoryEntry != NULL) {
                            DEBUG_VAR(directoryEntry->d_name, "%s");
                            DEBUG_VAR(directoryEntry->d_type, "%d");
                            if (directoryEntry->d_name[0] != '.') {
                                if (DT_REG == directoryEntry->d_type) {
                                    char urlNewRemoteFileName[PATH_MAX];
                                    sprintf(urlNewRemoteFileName,"%s/%s",urlRemoteDirectoryName,directoryEntry->d_name);
                                    DEBUG_VAR(urlNewRemoteFileName,"%s");
                                    error = copyFile(directoryEntry->d_name,urlNewRemoteFileName);
                                } else if (DT_DIR == directoryEntry->d_type) {
                                    char urlNewRemoteDirectoryName[PATH_MAX];
                                    const char *basename = strrchr(localDirectoryName, '/');
                                    if (basename != NULL) {
                                        basename++;
                                    } else {
                                        basename = localDirectoryName;
                                    }
                                    sprintf(urlNewRemoteDirectoryName, "%s/%s", urlRemoteDirectoryName, directoryEntry->d_name);
                                    DEBUG_VAR(urlNewRemoteDirectoryName, "%s");
                                    error = copyDirectory(directoryEntry->d_name, urlNewRemoteDirectoryName);
                                } else {
                                    INFO_MSG("entry %s type %d not supported", directoryEntry->d_name, directoryEntry->d_type);
                                }
                            } else {
                                DEBUG_MSG("%s skipped", directoryEntry->d_name);
                            }
                        } else {
                            if (errno != EXIT_SUCCESS) {
                                error = errno;
                                ERROR_MSG("readdir %s error %d (%m)", localDirectoryName, error);
                            }
                        }
                    } while ((EXIT_SUCCESS == error) && (directoryEntry != NULL));
                    
                    if (chdir(currentLocalDirectory) != 0) {
                        if (EXIT_SUCCESS == error) {
                            error = errno;
                        }
                        ERROR_MSG("chdir %s error %d (%m)", currentLocalDirectory, errno);
                    }
                } else {
                    error = errno;
                    ERROR_MSG("chdir %s error %d (%m)", localDirectoryName,error);
                }

                if (closedir(directory) == -1) {
                    if (EXIT_SUCCESS == error) {
                        error = errno;
                    }
                    ERROR_MSG("closedir %s error %d (%m)", localDirectoryName, errno);
                }
            } else {
                error = errno;
                ERROR_MSG("opendir %s error %d (%m)", localDirectoryName, error);
            }
        } /* error already logged */
    } else {
        error = errno;
        ERROR_MSG("getcwd error %d (%m)", error);
    }
    return error;
}

int smbCopyDirectory(const char *directoryname, const char* server, const char* remotePath) {
    int error = smbcInit();

    DEBUG_VAR(directoryname, "%s");
    DEBUG_VAR(server, "%s");
    DEBUG_VAR(remotePath, "%s");
    if (0 == error) {
        /* create remote directory url (work version) */
        char SMBWorkDirPath[PATH_MAX];

        const char *baseName = strrchr(directoryname, '/');
        if (baseName != NULL) {
            baseName++;
        } else {
            baseName = directoryname;
        }
        DEBUG_VAR(baseName, "%s");
        sprintf(SMBWorkDirPath, "smb://%s/%s/%s"IN_PROGRESS_EXTENSION, server, remotePath, baseName);
        DEBUG_VAR(SMBWorkDirPath, "%s");

        error = copyDirectory(directoryname, SMBWorkDirPath);
        if (EXIT_SUCCESS == error) {
            char SMBDirPath[PATH_MAX];
            sprintf(SMBDirPath, "smb://%s/%s/%s", server, remotePath, baseName);
            error = smbc_rename(SMBWorkDirPath, SMBDirPath);
            if (0 == error ) {
                char newName[PATH_MAX]; 
                DEBUG_MSG("%s successfully renamed to %s ",SMBWorkDirPath,SMBDirPath);
                
                sprintf(newName,"%s"COMPLETED_EXTENSION,directoryname); /* tempory name to set status transfert done before removing it */
                if (rename(directoryname,newName) == 0) {
                    if (isRemoveAfterTransfertSet(configuration)) {
                        error = removeDirectory(newName,1); /* error already logged */
                    }
                } else {
                    error = errno;
                    ERROR_MSG("rename %s => %s error %d (%m)",directoryname,newName,error);
                    if (isRemoveAfterTransfertSet(configuration)) {
                        /* anyway try to remove it */
                        error = removeDirectory(directoryname,1); /* error already logged */
                    }
                }
            } else {
                error = errno;
                ERROR_MSG("rename %s => %s error %d (%m)", SMBWorkDirPath, SMBDirPath, error);
                //TODO: manage EEXIST error (remove previous and restart rename op)
            }
        } /* error already logged */
    } /* error already logged */
    return error;
}

int smbRename(const char *oldName, const char *newName,const char* server) {
    int error = smbcInit();
  
    if (0 == error) {
        char oldSMBName[PATH_MAX];
        char newSMBName[PATH_MAX];           
                   
        sprintf(oldSMBName,"smb://%s/%s",server,oldName);
        DEBUG_VAR(oldSMBName,"%s");
        sprintf(newSMBName,"smb://%s/%s",server,newName);
        DEBUG_VAR(newSMBName,"%s");

        error = smbc_rename(oldSMBName, newSMBName);
        if (error != 0) {
            error = errno;
            ERROR_MSG("rename %s => %s error %d (%m)",oldSMBName,newSMBName,error);
        } else {
            INFO_MSG("%s successfully renamed to %s ",oldSMBName,newSMBName);
        }
    } /* error already logged */
    return error;
}

int smbUnlink(const char *name,const char* server) {
    int error = smbcInit();
    if (0 == error) {
        char SMBUrl[PATH_MAX];
        sprintf(SMBUrl,"smb://%s/%s",server,name);
        DEBUG_VAR(SMBUrl,"%s");
        error = smbc_unlink(SMBUrl);
        if (error != 0) {
            ERROR_MSG("unlink %s error (%d)",SMBUrl,error);
        } else {
            INFO_MSG("%s successfully deleted",SMBUrl);
        }
    } /* error already logged */
    return error;
}

int smbResumeCopy(const char *directoryname, const char *server, const char *remotePath) {
    int error = smbcInit();

    if (0 == error) {
        int directoryHandle = -1;
        char SMBUrl[PATH_MAX];
        sprintf(SMBUrl, "smb://%s/%s", server, remotePath);
        DEBUG_VAR(SMBUrl, "%s");

        directoryHandle = smbc_opendir(SMBUrl);
        if (directoryHandle >= 0) {
            unsigned char buffer[(sizeof(struct smbc_dirent) + 20)*20];
            struct smbc_dirent *directories = (struct smbc_dirent *) buffer;
            int size = 0;
            do {
                size = smbc_getdents(directoryHandle, directories, sizeof(buffer));
                if (size > 0) {                    
                    unsigned char *cursor = buffer;
                    unsigned char *limit = buffer + size;
                    while (cursor < limit) {
                        struct smbc_dirent *dirEntry = (struct smbc_dirent *) cursor;                        
                        if ((dirEntry->name[0] != '.') && ((SMBC_DIR == dirEntry->smbc_type) || (SMBC_FILE == dirEntry->smbc_type))) {
                            const char *extension = strrchr(dirEntry->name, '.');
                            DEBUG_VAR(dirEntry->name,"%s");
                            DEBUG_VAR(dirEntry->smbc_type,"%d");
                            if (extension != NULL) {
                                DEBUG_VAR(extension,"%s");
                                if (strcmp(extension+1, IN_PROGRESS_EXTENSION) != 0) {
                                    char localName[PATH_MAX];
                                    const int p = sprintf(localName,"%s/",directoryname);
                                    const size_t n = extension - dirEntry->name;                                    
                                    strncpy(localName+p, dirEntry->name, n);
                                    localName[n+p] = '\0';
                                    DEBUG_MSG("resuming transfert of %s", localName);

                                    if (SMBC_DIR == dirEntry->smbc_type) {
                                        error = smbCopyDirectory(localName, server, remotePath);
                                        /* error already logged */
                                    } else  {
                                        error = smbCopyFile(localName, server, remotePath);
                                        if (EXIT_SUCCESS == error) {
                                            if (isRemoveAfterTransfertSet(configuration)) {
                                                if (unlink(localName) != 0) {
                                                    error = errno;
                                                    ERROR_MSG("unlink %s error %d (%m)",localName,error);
                                                }
                                            } else {
                                                DEBUG_MSG("file %s successfully transfered and deleted",localName);
                                            }
                                        } /* error already logged */
                                    }
                                    DEBUG_MSG("resume transfert of %s status %d (%m)", localName,error);
                                } /* (strcmp(extension,IN_PROGRESS_EXTENSION) != 0) */
                            } /* (extension != NULL) */
                        } /* ((SMBC_DIR == cursor->smbc_type) || (SMBC_FILE == cursor->smbc_type)) */
                        cursor += dirEntry->dirlen;
                    } /* while ( cursor < limit) */
                } else if (size < 0) {
                    error = errno;
                    ERROR_MSG("smbc_getdents %s error %d (%m)", SMBUrl, error);
                }
            } while (size > 0);

            if (smbc_closedir(directoryHandle) < 0) {
                error = errno;
                ERROR_MSG("closedir %s error %d (%m)", SMBUrl, error);
            }
        } else {
            error = errno;
            ERROR_MSG("opendir %s error %d (%m)", SMBUrl, error);
        }
    } /* error already logged */
    return error;
}
