#ifndef _SO_VIEW_H_
#define _SO_VIEW_H_

#include <semaphore.h>

char *readFilepath(int pipefd, char *oldFilepath, sem_t *semaphore);
void processFile(int pipefd, char *filepath);
void sendSolution(int pipefd, char *solution);

#endif
