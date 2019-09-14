#ifndef _APPLICATION_H_
#define _APPLICATION_H_

int getSlavesQuantity(int filesSize);
void saveFile(int fd, int count, SatStruct *satStruct);
int createAndOpenPipe(char *name);
void sendFile(int fd, char *str, int *filesSent);
void processInput(int fd, SatStruct *satStructs, char *fileName, int index);
void terminateSlave(int fd);
void terminateView(SatStruct *satStructs, int count, sem_t *solvedSemaphore);

#endif