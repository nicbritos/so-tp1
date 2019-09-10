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

int main(int argc, char **argv) {
    int filesSize = argc - 1;
    if (filesSize < 1) {
        printError("You need to provide at least one CNF file.");
        exit(-1);
    }

    int sharedMemoryfd = shm_open(SHARED_SAT_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
    if (sharedMemoryfd == -1) {
        perror("Could not create shared memory object: ");
        exit(-1);
    }
    if (ftruncate(sharedMemoryfd, sizeof(SatStruct) * filesSize) == -1) {
        perror("Could not expand shared memory object: ");
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(-1);
    }
    SatStruct *map = (SatStruct*) mmap(NULL, sizeof(SatStruct) * filesSize, PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryfd, 0);
    if (map == NULL) {
        perror("Could not map shared memory: ");
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(-1);
    }

    sem_t *solvedSemaphore = sem_open(SHARED_SOLVED_SAT_SEMAPHORE_NAME, O_CREAT | O_RDWR);
    if (solvedSemaphore == SEM_FAILED) {
        perror("Could not create shared semaphore: ");
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(-1);
    }

    // Crea FIFO (named pipe)
    if (mkfifo(SHARED_PIPE_SAT_NPIPE_FILE, 0666) == -1) {
        perror("Could not create named pipe: ");
        closeSemaphore(solvedSemaphore, SHARED_SOLVED_SAT_SEMAPHORE_NAME);
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(-1);
    }

    int pipefd = open(myfifo, O_RDWR);
    if (pipefd == -1) {
        perror("Could not open named pipe: ");
        closeSemaphore(solvedSemaphore, SHARED_SOLVED_SAT_SEMAPHORE_NAME);
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(-1);
    }

    int outfd = open(OUT_FILE, O_CREAT | O_RDONLY, 0666);
    if (outfd == -1) {
        perror("Could not create output file: ");
        closeSemaphore(solvedSemaphore, SHARED_SOLVED_SAT_SEMAPHORE_NAME);
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        closePipe(pipefd);
        exit(-1);
    }


    exit(0);
}
