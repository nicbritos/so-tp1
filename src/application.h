#ifndef _APPLICATION_H_
#define _APPLICATION_H_

typedef struct AppStruct {
	SlaveStruct *slaveStructs;
	int slavesCount;

	SatStruct *satStructs;

	int filesSolved;
	int filesSize;
	int filesSent;
	char **files;

    fd_set readfds;

	char *viewSemaphoreName;
	sem_t *viewSemaphore;
	
	char *shmName;
	int shmfd;
	
	int outputfd;
} AppStruct;

int getSlavesQuantity(int filesSize);
void saveFile(int fd, int count, SatStruct *satStruct);
int createAndOpenPipe(char *name);
void sendFile(int fd, char *str);
void processInput(int fd, SatStruct *satStructs, char *fileName, int index);
void terminateSlave(int fd);
void terminateView(SatStruct *satStructs, int count, sem_t *solvedSemaphore);
void initializeAppStruct(AppStruct *appStruct, char **files, int filesSize);
void initializeSlaves(AppStruct *appStruct);
void shutdown(AppStruct *appStruct, int exitCode);

#endif