#ifndef _SO_VIEW_H_
#define _SO_VIEW_H_

#include <semaphore.h>

char *readFilepath(int pipefd, char *oldFilepath, sem_t *semaphore, long *fileId);
void processFile(int pipefd, char *filepath, long fileId);
void sendSolution(int pipefd, char *solution, int size);

#endif
