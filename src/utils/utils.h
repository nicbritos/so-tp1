#ifndef _SO_UTILS_H_
#define _SO_UTILS_H_

#include <semaphore.h>

void printError(char *s);

// Gets all filepaths that match regex expression.
// The list is null terminated and must be freed!
char **getFiles(int size, char **list, char *regex);

void closeSharedMemory(int fd, char *name);

void closeSemaphore(sem_t *sem, char *name);

void closePipe(int fd);

int min(int a, int b);

#endif
