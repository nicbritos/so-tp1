#ifndef _SO_VIEW_H_
#define _SO_VIEW_H_

#include <semaphore.h>

#include "utils/satStruct.h"

typedef struct ViewStruct {
	size_t fileNameShmSize;
	long fileNameShmIndex;
	char *fileNameShmName;
	char *fileNameShmMap;
	int fileNameShmfd;

	SatStruct *satStructs;
	size_t satShmSize;
	char *satShmName;
	char *satShmMap;
	int satShmfd;

	long filesSize;

	char *semaphoreName;
	sem_t *semaphore;
} ViewStruct;

void processResult(SatStruct satStruct, ViewStruct *viewStruct);
void shutdown(ViewStruct *viewStruct, int exitCode);
void initializeViewStruct(ViewStruct *viewStruct, long unsigned pid);

#endif
