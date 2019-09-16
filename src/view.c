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

    //Open shared memory
    char *fileNameShmName = calloc(sizeof(char), MAX_SHARED_MEMORY_NAME_LENGTH);
    snprintf(fileNameShmName, MAX_SHARED_MEMORY_NAME_LENGTH, SHARED_MEMORY_VIEW_FILENAME_FILE, pid);
    int fileNameShmfd = shm_open(fileNameShmName, O_RDONLY, READ_PERM);
    if (fileNameShmfd == -1) {
        perror("Could not open shared memory object: ");
        exit(ERROR_SHMOPEN_FAIL);
    }
    
    long fileNameShmMapSize = INITAL_FILENAME_SHM_MAP_SIZE;
    char *fileNameShmMap = mmap(NULL, fileNameShmMapSize, PROT_READ, MAP_SHARED, fileNameShmfd, 0);
    if (fileNameShmMap == MAP_FAILED) {
        perror("Could not map shared memory: ");
        close(fileNameShmfd);
        exit(ERROR_MMAP_FAIL);
    }

    //Open shared memory
    char *sharedMemoryName = calloc(sizeof(char), MAX_SHARED_MEMORY_NAME_LENGTH);
    snprintf(sharedMemoryName, MAX_SHARED_MEMORY_NAME_LENGTH, SHARED_MEMORY_VIEW_FILE, pid);
    int sharedMemoryfd = shm_open(sharedMemoryName, O_RDONLY, READ_PERM);
    if (sharedMemoryfd == -1) {
        perror("Could not open shared memory object: ");
        exit(ERROR_SHMOPEN_FAIL);
    }
    
    char *map = mmap(NULL, sizeof(long), PROT_READ, MAP_SHARED, sharedMemoryfd, 0);
    if (map == MAP_FAILED) {
        perror("Could not map shared memory: ");
        close(sharedMemoryfd);
        exit(ERROR_MMAP_FAIL);
    }

    long filesSize = *((long*) map);
    map = mremap(map, sizeof(long), sizeof(long) + filesSize * sizeof(SatStruct), MREMAP_MAYMOVE);
    if (map == MAP_FAILED) {
        perror("Could not expand shared memory map");
        exit(ERROR_MREMAP_FAIL);
    }
    SatStruct *satStructs = (SatStruct*)(map + sizeof(long));

    //Open shared semaphore
    char *sharedSemaphoreName = calloc(1, sizeof(char) * MAX_SEMAPHORE_NAME_LENGTH);
    snprintf(sharedSemaphoreName, MAX_SEMAPHORE_NAME_LENGTH, SHARED_SEMAPHORE_VIEW_FILE, pid);
    sem_t *solvedSemaphore = sem_open(sharedSemaphoreName, O_RDWR);
    if (solvedSemaphore == SEM_FAILED) {
        perror("Could not open shared semaphore: ");
        close(sharedMemoryfd);
        exit(ERROR_SEMOPEN_FAIL);
    }

    // Print available data
    int finished = 0;
    long count = 0, textIndex = 0;
    while (count != filesSize && !finished) {
        sem_wait(solvedSemaphore);

        SatStruct *satStruct = satStructs + count;
        if (satStruct->fileNameLength != 0) {
            processResult(*satStruct, &fileNameShmMap, &fileNameShmMapSize, &textIndex);
            count++;
        } else {
            printf("AAA\n");
            finished = 1;
        }
    }

    sem_close(solvedSemaphore);
    close(sharedMemoryfd);
    close(fileNameShmfd);
    munmap(map, sizeof(long) + filesSize * sizeof(char));
    munmap(fileNameShmMap, + fileNameShmMapSize * sizeof(char));

    exit(0);
}

void processResult(SatStruct satStruct, char **strmap, long *strmapSize, long *textIndex) {
    satStruct.fileName = malloc(sizeof(*satStruct.fileName) * (satStruct.fileNameLength + 1)); // This is a local copy!

    char *newStrMap = mremap(*strmap, *strmapSize, *strmapSize + satStruct.fileNameLength, MREMAP_MAYMOVE);
    if (newStrMap == MAP_FAILED) {
        perror("Could not expand shared memory map");
        exit(ERROR_MREMAP_FAIL);
    }
    *strmap = newStrMap;

    strncpy(satStruct.fileName, *strmap + *textIndex, satStruct.fileNameLength);
    satStruct.fileName[satStruct.fileNameLength] = '\0';

    *textIndex += satStruct.fileNameLength;
    *strmapSize += satStruct.fileNameLength;

    dumpResults(STDOUT_FD, &satStruct);

    free(satStruct.fileName);
}

