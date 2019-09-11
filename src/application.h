#ifndef _APPLICATION_H_
#define _APPLICATION_H_

int getSlavesQuantity(int filesSize);
void closeSharedMemory(int fd, char *name);
void closeSemaphore(sem_t *sem, char *name);
void closePipe(int fd);
void saveFile(int fd, int count, SatStruct *satStruct);

#endif