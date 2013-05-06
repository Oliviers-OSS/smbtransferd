#include "globals.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include "filesystem.h"

#define MODULE_FLAG FLAG_RUNTIME

int createDirectory(const char *dirname, unsigned int force) {
    int error = EXIT_SUCCESS;
    const mode_t mode = 0777;

    if (force) {
        char currentPath[PATH_MAX];
        char *currentPos = NULL;

        strcpy(currentPath, dirname);        
        currentPos = strchr(currentPath+1, '/'); /* +1 to manage absolute/relative path */
                
        while (currentPos != NULL) {
            *currentPos = '\0';
            if (mkdir(currentPath, mode) != 0) {
                error = errno;
                if (EEXIST == error) {
                    DEBUG_MSG("directory %s already exists", currentPath);
                } else {
                    ERROR_MSG("mkdir %s error %d (%m)", currentPath, error);
                }
            }
            *currentPos = '/';
            currentPos++;
            currentPos = strchr(currentPos, '/');
        } /* (currentPos != NULL) */
    } /* (force) */

    if (mkdir(dirname, mode) != 0) {
        error = errno;
        if (EEXIST == error) {
            DEBUG_MSG("directory %s already exists", dirname);
        } else {
            ERROR_MSG("mkdir %s error %d (%m)", dirname, error);
        }
    } else {
        error = EXIT_SUCCESS;
    }
    return error;
}

static inline int emptyDirectory(const char *directoryname) {
  int error = EXIT_SUCCESS;
  char currentLocalDirectory[PATH_MAX];
  if (getcwd(currentLocalDirectory, sizeof(currentLocalDirectory)) != NULL) {
      DIR *directory = opendir(directoryname);
      if (directory != NULL) {
         if (chdir(directoryname) == 0) {
             struct dirent *directoryEntry = NULL;
             errno = EXIT_SUCCESS; 
             do
             {
                directoryEntry = readdir(directory);
                if (directoryEntry != NULL) {
                   DEBUG_VAR(directoryEntry->d_name,"%s");
                   DEBUG_VAR(directoryEntry->d_type,"%d");
                   if (DT_DIR == directoryEntry->d_type) {
                       // (directoryEntry->d_name[0] == '.') && (directoryEntry->d_name[1] == '.') || (directoryEntry->d_name[1] == '\0'))
                       if ((directoryEntry->d_name[0] != '.') || ((directoryEntry->d_name[1] != '.') && (directoryEntry->d_name[1] != '\0'))) {
                           error = emptyDirectory(directoryEntry->d_name);
                           if (EXIT_SUCCESS == error) {
                              error = removeDirectory(directoryEntry->d_name,0);
                              /* error already logged */
                           } 
                       } else {
                           DEBUG_MSG("%s skipped",directoryEntry->d_name);
                       }
                   } else {
                     if (unlink(directoryEntry->d_name) != 0) {
                        error = errno;
                        ERROR_MSG("unlink %s error %d (%m)",directoryEntry->d_name,error);
                     } else {
                         DEBUG_MSG("%s deleted",directoryEntry->d_name);
                     }
                   }
                } else {
                  error = errno;
                  if (error != EXIT_SUCCESS) {
                      ERROR_MSG("readdir %s error %d (%m)",directoryname,error);
                  }
                }
             } while ((EXIT_SUCCESS == error) && (directoryEntry != NULL));

             if (chdir(currentLocalDirectory) != 0){
                 if (EXIT_SUCCESS == error) {
                     error = errno;
                 }
                 ERROR_MSG("chdir %s error %d (%m)",currentLocalDirectory,errno);
             }
         } else {
             error = errno;
             ERROR_MSG("chdir %s error %d (%m)",directoryname,error);
         }

         if (closedir(directory) == -1) {
            if (EXIT_SUCCESS == error) {
               error = errno;
            }
            ERROR_MSG("closedir %s error %d (%m)",directoryname,errno);
         }
      } else {
        error = errno;
        ERROR_MSG("opendir %s error %d (%m)",directoryname,error);
      }
  } else {
        error = errno;
        ERROR_MSG("getcwd error %d (%m)", error);
    }
  return error;
}

int removeDirectory(const char *dirname, unsigned int force) {
  int error = EXIT_SUCCESS;

  if (force) {
     error = emptyDirectory(dirname);
  } /* error already logged */

  if (EXIT_SUCCESS == error) {
     if (rmdir(dirname) != 0) {
        error = errno;
        ERROR_MSG("rmdir %s error %d (%m)",dirname,error);
     }
  }
  return error;
}
