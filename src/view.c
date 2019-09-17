#define _GNU_SOURCE

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <semaphore.h>

#include "utils/satStruct.h"
#include "utils/commonDef.h"
#include "utils/utils.h"
#include "view.h"

int main(int argc, char **argv) {
    long unsigned pid;

    if (argc < 2) {
        fflush(stdin);
        if (scanf("%lu", &pid) != 1) {
            printError("Invalid PID");
            exit(ERROR_NO_APPLICATION_PID);
        }
    } else {
        pid = atol(argv[1]);
    }

    ViewStruct viewStruct;

    initializeViewStruct(&viewStruct,  pid);

    // Print available data
    int finished = 0;
    long count = 0;
    while (count != viewStruct.filesSize && !finished) {
        sem_wait(viewStruct.semaphore);

        SatStruct *satStruct = viewStruct.satStructs + count;
        if (satStruct->fileNameLength != 0) {
            processResult(*satStruct, &viewStruct);
            count++;
        } else {
            finished = 1;
        }
    }

    shutdown(&viewStruct, ERROR_NO);
}

void processResult(SatStruct satStruct, ViewStruct *viewStruct) {
    satStruct.fileName = malloc(sizeof(*satStruct.fileName) * (satStruct.fileNameLength + 1)); // This is a local copy!

    char *newStrMap = mremap(viewStruct->fileNameShmMap, viewStruct->fileNameShmSize, viewStruct->fileNameShmSize + satStruct.fileNameLength, MREMAP_MAYMOVE);
    if (newStrMap == MAP_FAILED) {
        perror("Could not expand shared memory map");
        shutdown(viewStruct, ERROR_MREMAP_FAIL);
    }
    viewStruct->fileNameShmMap = newStrMap;
    viewStruct->fileNameShmSize += satStruct.fileNameLength;

    strncpy(satStruct.fileName, viewStruct->fileNameShmMap + viewStruct->fileNameShmIndex, satStruct.fileNameLength);
    satStruct.fileName[satStruct.fileNameLength] = '\0';

    viewStruct->fileNameShmIndex += satStruct.fileNameLength;

    dumpResults(STDOUT_FD, &satStruct);

    free(satStruct.fileName);
}

void shutdown(ViewStruct *viewStruct, int exitCode) {
    if (viewStruct->fileNameShmMap != NULL) {
        munmap(viewStruct->fileNameShmMap, viewStruct->fileNameShmSize);
        viewStruct->fileNameShmMap = NULL;
    }
    if (viewStruct->satShmMap != NULL) {
        munmap(viewStruct->satShmMap, viewStruct->satShmSize);
        viewStruct->satStructs = NULL;
        viewStruct->satShmMap = NULL;
    }
    if (viewStruct->semaphore != NULL) {
        sem_close(viewStruct->semaphore);
        viewStruct->semaphore = NULL;
    }
    if (viewStruct->semaphoreName != NULL) {
        free(viewStruct->semaphoreName);
        viewStruct->semaphoreName = NULL;
    }
    if (viewStruct->satShmfd != -1) {
        close(viewStruct->satShmfd);
        viewStruct->satShmfd = -1;
    }
    if (viewStruct->fileNameShmfd != -1) {
        close(viewStruct->fileNameShmfd);
        viewStruct->fileNameShmfd = -1;
    }
    if (viewStruct->fileNameShmName != NULL) {
        free(viewStruct->fileNameShmName);
        viewStruct->fileNameShmName = NULL;
    }
    if (viewStruct->satShmName != NULL) {
        free(viewStruct->satShmName);
        viewStruct->satShmName = NULL;
    }

    exit(exitCode);
}

void initializeViewStruct(ViewStruct *viewStruct, long unsigned pid) {
    viewStruct->fileNameShmSize = 0;
    viewStruct->fileNameShmIndex = 0;
    viewStruct->fileNameShmName = NULL;
    viewStruct->fileNameShmMap = NULL;
    viewStruct->fileNameShmfd = -1;

    viewStruct->satStructs = NULL;
    viewStruct->satShmSize = 0;
    viewStruct->satShmName = NULL;
    viewStruct->satShmMap = NULL;
    viewStruct->satShmfd = -1;

    viewStruct->filesSize = 0;

    viewStruct->semaphoreName = NULL;
    viewStruct->semaphore = NULL;

    //Open shared memory
    viewStruct->fileNameShmName = calloc(sizeof(char), MAX_SHARED_MEMORY_NAME_LENGTH);
    if (viewStruct->fileNameShmName == NULL) {
        shutdown(viewStruct, ERROR_ALLOC_MEMORY);
    }
    snprintf(viewStruct->fileNameShmName, MAX_SHARED_MEMORY_NAME_LENGTH, SHARED_MEMORY_VIEW_FILENAME_FILE, pid);
    viewStruct->fileNameShmfd = shm_open(viewStruct->fileNameShmName, O_RDONLY, READ_PERM);
    if (viewStruct->fileNameShmfd == -1) {
        perror("Could not open shared memory object");
        shutdown(viewStruct, ERROR_SHMOPEN_FAIL);
    }
    
    viewStruct->fileNameShmSize = INITAL_FILENAME_SHM_MAP_SIZE;
    viewStruct->fileNameShmMap = mmap(NULL, viewStruct->fileNameShmSize, PROT_READ, MAP_SHARED, viewStruct->fileNameShmfd, 0);
    if (viewStruct->fileNameShmMap == MAP_FAILED) {
        perror("Could not map shared memory: ");
        shutdown(viewStruct, ERROR_MMAP_FAIL);
    }

    //Open shared memory
    viewStruct->satShmName = calloc(sizeof(char), MAX_SHARED_MEMORY_NAME_LENGTH);
    if (viewStruct->satShmName == NULL) {
        shutdown(viewStruct, ERROR_ALLOC_MEMORY);
    }
    snprintf(viewStruct->satShmName, MAX_SHARED_MEMORY_NAME_LENGTH, SHARED_MEMORY_VIEW_FILE, pid);
    viewStruct->satShmfd = shm_open(viewStruct->satShmName, O_RDONLY, READ_PERM);
    if (viewStruct->satShmfd == -1) {
        perror("Could not open shared memory object: ");
        shutdown(viewStruct, ERROR_SHMOPEN_FAIL);
    }
    
    viewStruct->satShmMap = mmap(NULL, sizeof(long), PROT_READ, MAP_SHARED, viewStruct->satShmfd, 0);
    if (viewStruct->satShmMap == MAP_FAILED) {
        perror("Could not map shared memory: ");
        shutdown(viewStruct, ERROR_MMAP_FAIL);
    }

    viewStruct->filesSize = *((long*) viewStruct->satShmMap);
    viewStruct->satShmMap = mremap(viewStruct->satShmMap, sizeof(long), sizeof(long) + viewStruct->filesSize * sizeof(SatStruct), MREMAP_MAYMOVE);
    if (viewStruct->satShmMap == MAP_FAILED) {
        perror("Could not expand shared memory map");
        shutdown(viewStruct, ERROR_MREMAP_FAIL);
    }
    viewStruct->satStructs = (SatStruct*) (viewStruct->satShmMap + sizeof(long));

    //Open shared semaphore
    viewStruct->semaphoreName = calloc(1, sizeof(char) * MAX_SEMAPHORE_NAME_LENGTH);
    if (viewStruct->semaphoreName == NULL) {
        shutdown(viewStruct, ERROR_ALLOC_MEMORY);
    }
    snprintf(viewStruct->semaphoreName, MAX_SEMAPHORE_NAME_LENGTH, SHARED_SEMAPHORE_VIEW_FILE, pid);
    viewStruct->semaphore = sem_open(viewStruct->semaphoreName, O_RDWR);
    if (viewStruct->semaphore == SEM_FAILED) {
        perror("Could not open shared semaphore: ");
        shutdown(viewStruct, ERROR_SEMOPEN_FAIL);
    }
}
