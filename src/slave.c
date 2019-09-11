#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "utils/utils.h"
#include "utils/satStruct.h"

#define SHARED_SAT_MEMORY_NAME "/tp1SatMemory"
#define SHARED_SOLVED_SAT_SEMAPHORE_NAME "/tp1SolvedSatSemaphore"
#define SHARED_PIPE_SAT_NPIPE_FILE "/tmp/tp1NamedPipe"
#define OUT_FILE "./tp1_output.txt"
#define READ_AND_WRITE_PERM 0666

#define ERROR_NO 0
#define ERROR_NO_FILES -1
#define ERROR_SHMOPEN_FAIL -2
#define ERROR_FTRUNCATE_FAIL -3
#define ERROR_MMAP_FAIL -4
#define ERROR_SEMOPEN_FAIL -5
#define ERROR_FIFO_CREATION_FAIL -6
#define ERROR_FIFO_OPEN_FAIL -7
#define ERROR_FILE_OPEN_FAIL -8

int main(int argc, char **argv) {
    int filesSize = argc - 1; // -1 porque argc tiene cantidad de parametros + 1
    if (filesSize < 1) {
        printError("You need to provide at least one CNF file.");
        exit(ERROR_NO_FILES);
    }

    int sharedMemoryfd = shm_open(SHARED_SAT_MEMORY_NAME, O_CREAT | O_RDWR, READ_AND_WRITE_PERM);
    if (sharedMemoryfd == -1) {
        perror("Could not create shared memory object: ");
        exit(ERROR_SHMOPEN_FAIL);
    }
    if (ftruncate(sharedMemoryfd, sizeof(SatStruct) * filesSize) == -1) {
        perror("Could not expand shared memory object: ");
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(ERROR_FTRUNCATE_FAIL);
    }
    SatStruct *map = (SatStruct*) mmap(NULL, sizeof(*map) * filesSize, PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryfd, 0);
    if (map == NULL) {
        perror("Could not map shared memory: ");
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(ERROR_MMAP_FAIL);
    }

    sem_t *solvedSemaphore = sem_open(SHARED_SOLVED_SAT_SEMAPHORE_NAME, O_CREAT | O_RDWR);
    if (solvedSemaphore == SEM_FAILED) {
        perror("Could not create shared semaphore: ");
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(ERROR_SEMOPEN_FAIL);
    }

    // Crea FIFO (named pipe)
    if (mkfifo(SHARED_PIPE_SAT_NPIPE_FILE, READ_AND_WRITE_PERM) == -1) {
        perror("Could not create named pipe: ");
        closeSemaphore(solvedSemaphore, SHARED_SOLVED_SAT_SEMAPHORE_NAME);
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(ERROR_FIFO_CREATION_FAIL);
    }

    int pipefd = open(SHARED_PIPE_SAT_NPIPE_FILE, O_RDWR);
    if (pipefd == -1) {
        perror("Could not open named pipe: ");
        closeSemaphore(solvedSemaphore, SHARED_SOLVED_SAT_SEMAPHORE_NAME);
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(ERROR_FIFO_OPEN_FAIL);
    }

    int outfd = open(OUT_FILE, O_CREAT | O_RDONLY, READ_AND_WRITE_PERM);
    if (outfd == -1) {
        perror("Could not create output file: ");
        closeSemaphore(solvedSemaphore, SHARED_SOLVED_SAT_SEMAPHORE_NAME);
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        closePipe(pipefd);
        exit(ERROR_FILE_OPEN_FAIL);
    }


    exit(ERROR_NO);


}