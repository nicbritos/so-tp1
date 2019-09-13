#ifndef _SO_UTILS_H_
#define _SO_UTILS_H_

#include <semaphore.h>
#include "satStruct.h"

void printError(char *s);

void dumpResults(int fd, SatStruct *satStruct);

void closeSharedMemory(int fd);

void closeSemaphore(sem_t *sem);

void closeMasterSharedMemory(int fd, char *name);

void closeMasterSemaphore(sem_t *sem, char *name);

void closePipe(int fd);

void unmapSharedMemory(void *addr, size_t length);

int min(int a, int b);

int digits(int n);

#endif
