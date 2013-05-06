#ifndef _SMB_COPY_H_
#define _SMB_COPY_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define IN_PROGRESS_EXTENSION    ".work"
#define COMPLETED_EXTENSION     ".done"

int smbCopyFile(const char *filename,const char* server, const char* remotePath);
int smbCopyDirectory(const char *directoryname,const char* server, const char* remotePath);
int smbUnlink(const char *name,const char* server);
int smbRename(const char *oldName, const char *newName,const char* server);
int smbResumeCopy(const char *directoryname,const char* server, const char* remotePath);

#ifdef __cplusplus
}
#endif

#endif /* _SMB_COPY_H_ */
