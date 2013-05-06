#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

int createDirectory(const char *dirname, unsigned int force);
int removeDirectory(const char *dirname, unsigned int force);

#endif /* _FILESYSTEM_H_ */
