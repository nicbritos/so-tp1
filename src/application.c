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
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>
#include "utils/utils.h"
#include "utils/slaveStruct.h"
#include "utils/satStruct.h"
#include "application.h"

#define SHARED_SAT_MEMORY_NAME "/tp1mem%lu"
#define SHARED_SAT_DATA_SEMAPHORE_NAME "/tp1sem%lu"
#define SHARED_PIPE_SAT_WRITE_FILE "/tmp/tp1PipeW"
#define SHARED_PIPE_SAT_READ_FILE "/tmp/tp1PipeR"
#define OUT_FILE "./tp1_output.txt"

#define PIPE_IN_BUFFER_SIZE 4096
#define READ_AND_WRITE_PERM 0666
#define WRITE_PERM 0444
#define READ_PERM 0222
#define FILES_PER_SLAVE 10.0
#define FILES_MIN_PERCENTAGE 0.05 
#define MAX_SHARED_MEMORY_NAME_LENGTH 256
#define MAX_SEMAPHORE_NAME_LENGTH 256
#define MAX_SLAVES 512 // Solves problem of "Too many files open"

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

    AppStruct appStruct;
    initializeAppStruct(&appStruct, argv + 1, filesSize);

    // Para proceso Vista
    // sleep(2)
    printf("%lu\n", getpid());

    initializeSlaves(&appStruct);

    while (appStruct.filesSolved < filesSize) {
        printf("preselect\n");
        int ready = select(appStruct.slavesCount, &(appStruct.readfds), NULL, NULL, NULL);
        if (ready == -1) {
            printf("SELECT ERROR\n");
        } else if (ready != 0) {
            printf("SELECT\n");
            for (int i = 0; i < appStruct.slavesCount; i++) {
                SlaveStruct slaveStruct = (appStruct.slaveStructs)[i];

                if (FD_ISSET(slaveStruct.readPipefd, &(appStruct.readfds))) {
                    FD_CLR(slaveStruct.readPipefd, &(appStruct.readfds));

                    processInput(slaveStruct.readPipefd, appStruct.satStructs, "FALTA", (appStruct.filesSolved)++);
                    if (appStruct.filesSent < filesSize) {
                        sendFile(slaveStruct.writePipefd, appStruct.files);
                        appStruct.filesSent++;
                    } else {
                        terminateSlave(slaveStruct.writePipefd);
                    }

                    sem_post(&(appStruct.viewSemaphore));
                }
            }
        }
    }

    shutdown(&appStruct, ERROR_NO);
}

int getSlavesQuantity(int filesSize) {
    return min(ceil(filesSize / FILES_PER_SLAVE), MAX_SLAVES);
}

int getMinFilesQuantity(int filesSize){
    return min(floor(filesSize * FILES_MIN_PERCENTAGE), floor(FILES_PER_SLAVE/2));
}

void saveFile(int fd, int count, SatStruct *satStructs) {
    dprintf(fd, "TP1 - MINISAT output for %d files\n", count);
    for (int i = 0; i < count; i++) {
        dumpResults(fd, satStructs + i);
    }
}

void sendFile(int fd, char *str) {
    write(fd, str, strlen(str));
}

// TODO: UTILS.C ?
void processInput(int fd, SatStruct *satStructs, char *fileName, int index) {
    SatStruct *satStruct = satStructs + index;
    char *data = readFromFile(fd);
    printf(data);
    int vars, clauses, sat;
    long cpuTime;
    scanf(data, "%d\n%d\n%ld\n%d", &(satStruct->variables), &(satStruct->clauses), &(satStruct->processingTime), &(satStruct->isSat));

    satStruct->filename = fileName;
    satStruct->processedBySlaveID = -1;
}

void terminateSlave(int fd) {
    write(fd, "", 1);
}

void terminateView(SatStruct *satStructs, int count, sem_t *solvedSemaphore) {
    SatStruct *satStruct = satStructs + count;
    satStruct->filename = NULL;
    sem_post(solvedSemaphore);
}

void initializeAppStruct(AppStruct *appStruct, char **files, int filesSize) {
    pid_t pid = getpid();

    appStruct->slaveStructs = NULL;
    appStruct->slavesCount = 0;

    appStruct->satStructs = NULL;

    appStruct->filesSolved = 0;
    appStruct->filesSize = filesSize;
    appStruct->filesSent = 0;
    appStruct->files = files;

    appStruct->viewSemaphoreName = NULL;
    appStruct->viewSemaphore = NULL;
    appStruct->shmName = NULL;
    appStruct->shmfd = -1;
    appStruct->outputfd = -1;

    // Create shared memory
    appStruct->shmName = calloc(1, sizeof(char) * MAX_SHARED_MEMORY_NAME_LENGTH);
    snprintf(appStruct->shmName, MAX_SHARED_MEMORY_NAME_LENGTH, SHARED_SAT_MEMORY_NAME, pid);
    appStruct->shmfd = shm_open(appStruct->shmName, O_CREAT | O_RDWR, READ_AND_WRITE_PERM);
    if (appStruct->shmfd == -1) {
        perror("Could not create shared memory object");
        shutdown(appStruct, ERROR_SHMOPEN_FAIL);
    }

    // Reserve storage for shared memory
    size_t satStructsSize = sizeof(SatStruct) * (filesSize + 1); // One more for the terminating struct
    if (ftruncate(appStruct->shmfd, satStructsSize) == -1) {
        perror("Could not expand shared memory object");
        shutdown(appStruct, ERROR_FTRUNCATE_FAIL);
    }

    // Map the shared memory
    appStruct->satStructs = (SatStruct*) mmap(NULL, satStructsSize, PROT_READ | PROT_WRITE, MAP_SHARED, appStruct->shmfd, 0);
    if (appStruct->satStructs == NULL) {
        perror("Could not map shared memory");
        shutdown(appStruct, ERROR_MMAP_FAIL);
    }

    // Create shared semaphore (View)
    appStruct->viewSemaphoreName = calloc(1, sizeof(char) * MAX_SEMAPHORE_NAME_LENGTH);
    snprintf(appStruct->viewSemaphoreName, MAX_SEMAPHORE_NAME_LENGTH, SHARED_SAT_DATA_SEMAPHORE_NAME, pid);
    sem_t *solvedSemaphore = sem_open(appStruct->viewSemaphoreName, O_CREAT | O_RDWR);
    if (solvedSemaphore == SEM_FAILED) {
        perror("Could not create shared semaphore");
        shutdown(appStruct, ERROR_SEMOPEN_FAIL);
    }

    // Create the output file
    appStruct->outputfd = open(OUT_FILE, O_CREAT | O_RDONLY, READ_AND_WRITE_PERM);
    if (appStruct->outputfd == -1) {
        perror("Could not create output file");
        shutdown(appStruct, ERROR_FILE_OPEN_FAIL);
    }
}

void initializeSlaves(AppStruct *appStruct) {
    pid_t pid = getpid();

    int slavesQuantity = getSlavesQuantity(appStruct->filesSize);
    int slavesQuantityDigits = digits(slavesQuantity);
    int writePipeNameLength = strlen(SHARED_PIPE_SAT_WRITE_FILE);
    int readPipeNameLength = strlen(SHARED_PIPE_SAT_READ_FILE);

    appStruct->slaveStructs = calloc(slavesQuantity, sizeof(SlaveStruct));
    FD_ZERO(&(appStruct->readfds));
    for (int i = 0; i < slavesQuantity; i++) {
        SlaveStruct slaveStruct = (appStruct->slaveStructs)[i];
        slaveStruct.id = i;
        appStruct->slavesCount += 1;

        // Create Pipes Name
        slaveStruct.writePipeName = malloc(sizeof(*(slaveStruct.writePipeName)) * (writePipeNameLength + slavesQuantityDigits + 1));
        strcpy(slaveStruct.writePipeName, SHARED_PIPE_SAT_WRITE_FILE);
        sprintf(slaveStruct.writePipeName + writePipeNameLength + slavesQuantityDigits - digits(i), "%d", i);
        for (int j = 0; j < slavesQuantityDigits - digits(i); j++) {
            slaveStruct.writePipeName[writePipeNameLength + j] = '0';
        }
        slaveStruct.readPipeName = malloc(sizeof(*(slaveStruct.readPipeName)) * (readPipeNameLength + slavesQuantityDigits + 1));
        strcpy(slaveStruct.readPipeName, SHARED_PIPE_SAT_READ_FILE);
        sprintf(slaveStruct.readPipeName + readPipeNameLength + slavesQuantityDigits - digits(i), "%d", i);
        for (int j = 0; j < slavesQuantityDigits - digits(i); j++) {
            slaveStruct.readPipeName[readPipeNameLength + j] = '0';
        }

        int forkResult = fork();
        if (forkResult > 0) {
            // Padre
            // Create FIFO (named pipe)
            if (mkfifo(slaveStruct.writePipeName, READ_AND_WRITE_PERM) == -1) {
                perror("Could not create named pipe");
                shutdown(appStruct, ERROR_FIFO_CREATION_FAIL);
            }
            // Create FIFO (named pipe)
            if (mkfifo(slaveStruct.readPipeName, READ_AND_WRITE_PERM) == -1) {
                perror("Could not create named pipe");
                shutdown(appStruct, ERROR_FIFO_CREATION_FAIL);
            }

            // Open the FIFO 
            slaveStruct.writePipefd = open(slaveStruct.writePipeName, O_WRONLY);
            if (slaveStruct.writePipefd == -1) {
                perror("Could not open named pipe");
                shutdown(appStruct, ERROR_FIFO_OPEN_FAIL);
            }
            slaveStruct.readPipefd = open(slaveStruct.readPipeName, O_RDONLY);
            if (slaveStruct.readPipefd == -1) {
                perror("Could not open named pipe");
                shutdown(appStruct, ERROR_FIFO_OPEN_FAIL);
            }
            FD_SET(slaveStruct.readPipefd, &(appStruct->readfds));

            sendFile(slaveStruct.writePipefd, appStruct->files);
            appStruct->filesSent++;
        } else if (forkResult == -1) {
            shutdown(appStruct, ERROR_FIFO_OPEN_FAIL);
        } else {
            // Hijo --> Instanciar cada slave (ver enunciado)
            char *slaveId = malloc(sizeof(*slaveId) * (slavesQuantityDigits + 1));
            sprintf(slaveId, "%d", i);
            char *arguments[4] = {slaveStruct.writePipeName, slaveStruct.readPipeName, slaveId, NULL};
            if (execvp("./src/slave", arguments) == -1) {
                // Error. TERMINAR
            }
        }
    }
}

void shutdown(AppStruct *appStruct, int exitCode) {
    if (appStruct->satStructs != NULL) {
        terminateView(appStruct->satStructs, appStruct->filesSolved, appStruct->viewSemaphore);
        size_t satStructsSize = sizeof(SatStruct) * (appStruct->filesSize + 1); // One more for the terminating struct
        munmap(appStruct->satStructs, satStructsSize);
    }
    if (appStruct->viewSemaphore != NULL) {
        sem_close(appStruct->viewSemaphore);
    }
    if (appStruct->viewSemaphoreName != NULL) {
        sem_unlink(appStruct->viewSemaphoreName);
        free(appStruct->viewSemaphoreName);
    }
    if (appStruct->outputfd != -1) {
        close(appStruct->outputfd);
    }
    if (appStruct->shmfd != -1) {
        close(appStruct->shmfd);
    }
    if (appStruct->shmName != NULL) {
        shm_unlink(appStruct->shmName);
        free(appStruct->shmName);
    }

    exit(exitCode);
}
