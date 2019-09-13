#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "utils/utils.h"
#include "utils/satStruct.h"
#include "view.h"

#define SHARED_SAT_MEMORY_NAME "/tp1mem%s"
#define SHARED_SAT_DATA_SEMAPHORE_NAME "/tp1sem%s"

#define READ_PERM 0222
#define MAX_SHARED_MEMORY_NAME_LENGTH 256
#define MAX_SEMAPHORE_NAME_LENGTH 256
#define STDOUT_FD 1

#define ERROR_NO 0
#define ERROR_NO_APPLICATION_PID -1
#define ERROR_SHMOPEN_FAIL -2
#define ERROR_FTRUNCATE_FAIL -3
#define ERROR_MMAP_FAIL -4
#define ERROR_SEMOPEN_FAIL -5
#define ERROR_FIFO_CREATION_FAIL -6
#define ERROR_FIFO_OPEN_FAIL -7
#define ERROR_FILE_OPEN_FAIL -8

int main(int argc, char **argv) {
    if (argc < 2) {
        printError("You need to provide the Applications' PID.");
        exit(ERROR_NO_APPLICATION_PID);
    }

    //Open shared memory
    char *sharedMemoryName = calloc(1, sizeof(char) * MAX_SHARED_MEMORY_NAME_LENGTH);
    snprintf(sharedMemoryName, MAX_SHARED_MEMORY_NAME_LENGTH, SHARED_SAT_MEMORY_NAME, argv[1]);
    int sharedMemoryfd = shm_open(sharedMemoryName, O_RDONLY, READ_PERM);
    if (sharedMemoryfd == -1) {
        perror("Could not open shared memory object: ");
        exit(ERROR_SHMOPEN_FAIL);
    }

    //Open shared semaphore
    char *sharedSemaphoreName = calloc(1, sizeof(char) * MAX_SEMAPHORE_NAME_LENGTH);
    snprintf(sharedSemaphoreName, MAX_SEMAPHORE_NAME_LENGTH, SHARED_SAT_DATA_SEMAPHORE_NAME, argv[1]);
    sem_t *solvedSemaphore = sem_open(sharedSemaphoreName, O_RDWR);
    if (solvedSemaphore == SEM_FAILED) {
        perror("Could not open shared semaphore: ");
        closeSharedMemory(sharedMemoryfd);
        exit(ERROR_SEMOPEN_FAIL);
    }

    // Print available data
    SatStruct *satStruct = NULL;
    int count = 0;
    int finished = 0;
    while (!finished) {
        sem_wait(solvedSemaphore);
        satStruct = getNextSatStruct(satStruct, sharedMemoryfd, count);
        if (satStruct->filename != NULL) {
            printResults(satStruct);
            count++;
        } else {
            finished = 1;
        }
    }

    closeSemaphore(solvedSemaphore);
    closeSharedMemory(sharedMemoryfd);
    if (satStruct != NULL)
        unmapSharedMemory(satStruct, sizeof(SatStruct));

    exit(0);
}

void printResults(SatStruct *satStruct) {
    dumpResults(STDOUT_FD, satStruct);
}

SatStruct* getNextSatStruct(SatStruct *oldMap, int sharedMemoryfd, int count) {
    if (oldMap != NULL) {
        unmapSharedMemory(oldMap, sizeof(SatStruct));
    }

    SatStruct *satStruct = (SatStruct*) mmap(NULL, sizeof(SatStruct), PROT_READ, MAP_SHARED, sharedMemoryfd, count * sizeof(SatStruct));
    if (satStruct == NULL) {
        perror("Could not map shared memory: ");
        closeSharedMemory(sharedMemoryfd);
        exit(ERROR_MMAP_FAIL);
    }

    return satStruct;
}
