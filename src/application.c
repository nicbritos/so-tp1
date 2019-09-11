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
#include "application.h"

#define SHARED_SAT_MEMORY_NAME "/tp1SatMemory"
#define SHARED_SOLVED_SAT_SEMAPHORE_NAME "/tp1SolvedSatSemaphore"
#define SHARED_PIPE_SAT_NPIPE_FILE "/tmp/tp1NamedPipe"
#define OUT_FILE "./tp1_output.txt"

#define PIPE_IN_BUFFER_SIZE 4096
#define READ_AND_WRITE_PERM 0666
#define FILES_PER_SLAVE 10.0
#define FILES_MIN_PERCENTAGE 0.05 

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
    int filesSize = argc - 1;
    if (filesSize < 1) {
        printError("You need to provide at least one CNF file.");
        exit(ERROR_NO_FILES);
    }

    //Create shared memory
    int sharedMemoryfd = shm_open(SHARED_SAT_MEMORY_NAME, O_CREAT | O_RDWR, READ_AND_WRITE_PERM);
    if (sharedMemoryfd == -1) {
        perror("Could not create shared memory object: ");
        exit(ERROR_SHMOPEN_FAIL);
    }

    //Reserve storage for shared memory
    if (ftruncate(sharedMemoryfd, sizeof(SatStruct) * filesSize) == -1) {
        perror("Could not expand shared memory object: ");
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(ERROR_FTRUNCATE_FAIL);
    }

    //Map the shared memory
    SatStruct *map = (SatStruct*) mmap(NULL, sizeof(SatStruct) * filesSize, PROT_READ | PROT_WRITE, MAP_SHARED, sharedMemoryfd, 0);
    if (map == NULL) {
        perror("Could not map shared memory: ");
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(ERROR_MMAP_FAIL);
    }

    //Create a shared semaphore
    sem_t *solvedSemaphore = sem_open(SHARED_SOLVED_SAT_SEMAPHORE_NAME, O_CREAT | O_RDWR);
    if (solvedSemaphore == SEM_FAILED) {
        perror("Could not create shared semaphore: ");
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(ERROR_SEMOPEN_FAIL);
    }

    // Create FIFO (named pipe)
    if (mkfifo(SHARED_PIPE_SAT_NPIPE_FILE, READ_AND_WRITE_PERM) == -1) {
        perror("Could not create named pipe: ");
        closeSemaphore(solvedSemaphore, SHARED_SOLVED_SAT_SEMAPHORE_NAME);
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(ERROR_FIFO_CREATION_FAIL);
    }

    // Open the FIFO 
    int pipefd = open(SHARED_PIPE_SAT_NPIPE_FILE, O_RDWR);
    if (pipefd == -1) {
        perror("Could not open named pipe: ");
        closeSemaphore(solvedSemaphore, SHARED_SOLVED_SAT_SEMAPHORE_NAME);
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        exit(ERROR_FIFO_CREATION_FAIL);
    }

    //Create the output file
    int outfd = open(OUT_FILE, O_CREAT | O_RDONLY, READ_AND_WRITE_PERM);
    if (outfd == -1) {
        perror("Could not create output file: ");
        closeSemaphore(solvedSemaphore, SHARED_SOLVED_SAT_SEMAPHORE_NAME);
        closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
        closePipe(pipefd);
        exit(ERROR_FILE_OPEN_FAIL);
    }

    // No se si esto o PID. Es para que lo reciba la app vista
    printf(SHARED_SAT_MEMORY_NAME);

    int slavesQuantity = getSlavesQuantity(filesSize);
    char **files = argv + 1;
    int satFinished = 0;
    for (int i = 0; i < slavesQuantity; i++) {
        int forkResult = fork();
        if (forkResult > 0) {
            // Padre -- No hace nada
        } else if (forkResult == -1) {
            // error
        } else {
            // Hijo --> Instanciar cada slave (ver enunciado)
            char *arguments[3] = {SHARED_PIPE_SAT_NPIPE_FILE, SHARED_SOLVED_SAT_SEMAPHORE_NAME, NULL};
            execvp("./slave.out", arguments);
        }
    }

    // TODO: Idle processes when there are no more files to be processed
    // Aca tenemos que esperar a que haya datos. Para esto, solo podemos hacer un wait y esperar a un post.
    // Ni idea como hacer esto despues
    char inBuffer[PIPE_IN_BUFFER_SIZE] = {0};
    while (satFinished < filesSize) {
        // Tenemos que ir llenando el buffer
        sem_wait(solvedSemaphore);
    }

    closeSemaphore(solvedSemaphore, SHARED_SOLVED_SAT_SEMAPHORE_NAME);
    closeSharedMemory(sharedMemoryfd, SHARED_SAT_MEMORY_NAME);
    closePipe(pipefd);

    saveFile(outfd, filesSize, map);
    close(outfd);

    exit(0);
}

int getSlavesQuantity(int filesSize) {
    return ceil(filesSize / FILES_PER_SLAVE);
}

int getMinFilesQuantity(int filesSize){
    return floor(filesSize * FILES_MIN_PERCENTAGE);
}

void closeSharedMemory(int fd, char *name) {
    close(fd);
    shm_unlink(name);
}

void closeSemaphore(sem_t *sem, char *name) {
    sem_close(sem);
    sem_unlink(name);
}

void closePipe(int fd) {
    close(fd);
}

void saveFile(int fd, int count, SatStruct *satStruct) {
    dprintf(fd, "TP1 - MINISAT output for %d files\n", count);
    for (int i = 0; i < count; i++) {
        dprintf(fd, 
            "Filename: %s\n"
             "Clauses: %d\n"
             "Variables: %d\n"
             "Satisfiable: %s\n"
             "Processing time: %ld\n"
             "Processed by Slave ID: %d\n\n", 
            satStruct->filename,
            satStruct->clauses,
            satStruct->variables,
            satStruct->isSat == 0 ? "UNSAT" : "SAT",
            satStruct->processingTime,
            satStruct->processedBySlaveID);
    }
}
