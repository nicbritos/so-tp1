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
	char *fileNameShmMap;
	char *fileNameShmMapName;
	size_t fileNameShmSize;
	size_t fileNameShmCount;
    int fileNameShmfd;

    struct pollfd *pollfdStructs;

	char *viewSemaphoreName;
	sem_t *viewSemaphore;
	
	char *shmName;
	int shmfd;
	
	int outputfd;

	long shmCursorPosition;
	long shmSize;
	char * shmContent;
} AppStruct;

int getSlavesQuantity(int filesSize);
void saveFile(int fd, int count, SatStruct *satStruct);
int createAndOpenPipe(char *name);
void sendFile(SlaveStruct *slaveStruct, char *fileName, long fileIndex);
void processInput(int fd, SatStruct *satStruct, AppStruct *appStruct, int slaveId);
void terminateSlave(SlaveStruct *slaveStruct);
void terminateView(SatStruct *satStructs, int count, sem_t *solvedSemaphore);
void initializeAppStruct(AppStruct *appStruct, char **files, int filesSize);
void initializeSlaves(AppStruct *appStruct);
void shutdown(AppStruct *appStruct, int exitCode);
void shutdownSlave(SlaveStruct *slaveStruct);
void writeFileNameToBuffer(AppStruct *appStruct, char *s, int length);

#endif