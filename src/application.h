#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <sys/poll.h>

typedef struct AppStruct {
	SlaveStruct *slaveStructs;
	int slavesQuantity;

	SatStruct *satStructs;

	long filesSolved;
	long filesSize;
	long filesSent;
	char **files;

    struct pollfd *pollfdStructs;

	char *viewSemaphoreName;
	sem_t *viewSemaphore;
	
	char *shmName;
	int shmfd;
	
	int outputfd;
} AppStruct;

int getSlavesQuantity(int filesSize);
void saveFile(int fd, int count, SatStruct *satStruct);
int createAndOpenPipe(char *name);
void sendFile(int fd, char *fileName, long fileIndex);
void processInput(int fd, SatStruct *satStruct, char **files, int slaveId);
void terminateSlave(int fd);
void terminateView(SatStruct *satStructs, int count, sem_t *solvedSemaphore);
void initializeAppStruct(AppStruct *appStruct, char **files, int filesSize);
void initializeSlaves(AppStruct *appStruct);
void shutdown(AppStruct *appStruct, int exitCode);

#endif