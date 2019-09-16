#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include "utils/utils.h"
#include "utils/satStruct.h"
#include "view.h"

#define SHARED_MEMORY_VIEW_FILE "/tp1ViewMem%lu"
#define SHARED_SEMAPHORE_VIEW_FILE "/tp1ViewSem%lu"

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

    long unsigned pid = atol(argv[1]);

    //Open shared memory
    char *sharedMemoryName = calloc(1, sizeof(char) * MAX_SHARED_MEMORY_NAME_LENGTH);
    snprintf(sharedMemoryName, MAX_SHARED_MEMORY_NAME_LENGTH, SHARED_MEMORY_VIEW_FILE, pid);
    int sharedMemoryfd = shm_open(sharedMemoryName, O_RDONLY, READ_PERM);
    if (sharedMemoryfd == -1) {
        perror("Could not open shared memory object: ");
        exit(ERROR_SHMOPEN_FAIL);
    }

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
    char *map = NULL;
    int count = 0;
    int finished = 0;
    char * output = NULL;
    while (!finished) {
        sem_wait(solvedSemaphore);
        map = getNextSolved(map, &output, sharedMemoryfd, &count);
        if (output != NULL) {
            printf("%s\n", output);
        } else {
            finished = 1;
        }
    }

    sem_close(solvedSemaphore);
    close(sharedMemoryfd);
    if (map != NULL)
        if(output != NULL)
            munmap(map, strlen(output));

    exit(0);
}

char* getNextSolved(char *map, char **output, int sharedMemoryfd, int * count) {
    printf("TOY\n");
    long separatorPosition = findSeparatorN(map, &count);
    if (map != NULL) {
        *count+= strlen(*output);
        munmap(map, strlen(*output));
    }
    map = (char*) mmap(NULL, sizeof(*map), PROT_READ, MAP_SHARED, sharedMemoryfd, *count);
    if (map == NULL) {
        perror("Could not map shared memory: ");
        close(sharedMemoryfd);
        exit(ERROR_MMAP_FAIL);
    }


    return map;
}

long findSeparatorN(char *s, int max){
    int count = 0;
    while((s[count] != 0) && count < max){
        if(s[count] == '\n')
            if(s[count+1] == '\n')
                return count+1;
        count++;
    }
    return -1;
}
