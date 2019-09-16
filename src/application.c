#define _GNU_SOURCE 1
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
#include "utils/errorDef.h"
#include "application.h"

#define SHARED_MEMORY_VIEW_FILE "/tp1ViewMem%lu"
#define SHARED_MEMORY_VIEW_FILENAME_FILE "/tp1ViewFileNameMem%lu"
#define SHARED_SEMAPHORE_VIEW_FILE "/tp1ViewSem%lu"
#define SHARED_SEMAPHORE_SAT_FILE "/tp1SlaveSem"
#define SHARED_PIPE_SAT_WRITE_FILE "/tmp/tp1SlavePipeW"
#define SHARED_PIPE_SAT_READ_FILE "/tmp/tp1SlavePipeR"
#define OUT_FILE "./tp1_output.txt"

#define INITAL_FILENAME_SHM_MAP_SIZE (sizeof(char))

#define INFINITE_POLL -1
#define PIPE_IN_BUFFER_SIZE 4096
#define READ_AND_WRITE_PERM 0666
#define WRITE_PERM 0444
#define READ_PERM 0222
#define FILES_PER_SLAVE 10.0
#define FILES_MIN_PERCENTAGE 0.05 
#define MAX_SHARED_MEMORY_NAME_LENGTH 256
#define MAX_SEMAPHORE_NAME_LENGTH 256
#define MAX_SLAVES 512 // Solves problem of "Too many files open"

int main(int argc, char **argv) {
    AppStruct appStruct;
    
    appStruct.filesSize = argc - 1;
    if (appStruct.filesSize < 1) {
        printError("You need to provide at least one CNF file.");
        exit(ERROR_NO_FILES);
    }

    initializeAppStruct(&appStruct, argv + 1, appStruct.filesSize);

    // Para proceso Vista
    printf("%lu\n", (long unsigned) getpid());
    fflush(stdout);

    initializeSlaves(&appStruct);

    while (appStruct.filesSolved < appStruct.filesSize) {
        int ready = poll(appStruct.pollfdStructs, appStruct.slavesQuantity, INFINITE_POLL);
        if (ready == -1) {
            // ERROR
        } else if (ready != 0) {
            for (int i = 0; i < appStruct.slavesQuantity; i++) {
                SlaveStruct slaveStruct = appStruct.slaveStructs[i];
                struct pollfd pollfdStruct = appStruct.pollfdStructs[i];

                if (pollfdStruct.revents & POLLIN) {
                    pollfdStruct.revents = 0;

                    processInput(slaveStruct.readPipefd, appStruct.satStructs + appStruct.filesSolved, &appStruct, slaveStruct.id);
                    appStruct.filesSolved++;
                    
                    // printf("Done: %ld\n", appStruct.filesSolved);
                    sem_post(appStruct.viewSemaphore);
                    if (appStruct.filesSent < appStruct.filesSize) {
                        // printf("continue\n");
                        sendFile(&slaveStruct, appStruct.files[appStruct.filesSent], appStruct.filesSent);
                        appStruct.filesSent++;
                        // int val;
                        // sem_getvalue(slaveStruct.fileAvailableSemaphore, &val);
                        // printf("Sem value pre: %d\n", val);
                        sem_post(slaveStruct.fileAvailableSemaphore);
                        // sem_getvalue(slaveStruct.fileAvailableSemaphore, &val);
                        // printf("Sem value post: %d\n", val);
                    } else {
                        terminateSlave(&slaveStruct);
                        // shutdownSlave(&slaveStruct);
                    }
                }
            }
        }
    }

    printf("save\n");
    saveFile(appStruct.outputfd, appStruct.filesSolved, appStruct.satStructs);
    printf("shutdown\n");
    sleep(1); // TODO: Fix race conditions. Wait for slave to disconnect!!!!!!!!
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

void sendFile(SlaveStruct *slaveStruct, char *fileName, long fileIndex) {
    int fileNameLength = strlen(fileName);
    int fileIndexDigits;
    if (fileIndex < 0) {
        fileIndexDigits = digits(-fileIndex) + 1;
    } else {
        fileIndexDigits = digits(fileIndex);
    }
    
    char *data = malloc(sizeof(*data) * (fileNameLength + 1 + fileIndexDigits + 2));
    sprintf(data, "%s\n%ld\n", fileName, fileIndex);
    write(slaveStruct->writePipefd, data, fileNameLength + fileIndexDigits + 2);
    free(data);
}

void processInput(int fd, SatStruct *satStruct, AppStruct *appStruct, int slaveId) {
    long fileIndex;
    char *data = readFromFile(fd);
    sscanf(data, "%d\n%d\n%f\n%d\n%ld\n", &(satStruct->variables), &(satStruct->clauses), &(satStruct->processingTime), &(satStruct->isSat), &fileIndex);
    free(data);

    satStruct->fileName = appStruct->files[fileIndex];
    satStruct->fileNameLength = strlen(satStruct->fileName);
    satStruct->processedBySlaveID = slaveId;

    writeFileNameToBuffer(appStruct, satStruct->fileName, satStruct->fileNameLength);
}

void writeFileNameToBuffer(AppStruct *appStruct, char *s, int length) {
    if (appStruct->fileNameShmSize - appStruct->fileNameShmCount < length) {
        size_t newSize = sizeof(*s) * (appStruct->fileNameShmSize + length);
        if (ftruncate(appStruct->fileNameShmfd, newSize) == -1) {
            perror("Could not expand shared memory object");
            shutdown(appStruct, ERROR_FTRUNCATE_FAIL);
        }
        char *newFileNameShmMap = mremap(appStruct->fileNameShmMap, appStruct->fileNameShmSize, appStruct->fileNameShmSize + length, MREMAP_MAYMOVE);
        if (newFileNameShmMap == MAP_FAILED) {
            perror("Could not expand shared memory map");
            shutdown(appStruct, ERROR_MREMAP_FAIL);
        }

        appStruct->fileNameShmMap = newFileNameShmMap;
        appStruct->fileNameShmSize += length;
    }

    strncpy(appStruct->fileNameShmMap + appStruct->fileNameShmCount, s, length);
    appStruct->fileNameShmCount += length;
}

void terminateSlave(SlaveStruct *slaveStruct) {
    sendFile(slaveStruct, "\n", -1);
    sem_post(slaveStruct->fileAvailableSemaphore);
}

void terminateView(SatStruct *satStructs, int count, sem_t *solvedSemaphore) {
    SatStruct *satStruct = satStructs + count;
    satStruct->fileName = NULL;
    sem_post(solvedSemaphore);
}

void initializeAppStruct(AppStruct *appStruct, char **files, int filesSize) {
    long unsigned pid = (long unsigned) getpid();

    appStruct->slaveStructs = NULL;
    appStruct->slavesQuantity = 0;

    appStruct->satStructs = NULL;

    appStruct->filesSolved = 0;
    appStruct->filesSize = filesSize;
    appStruct->filesSent = 0;
    appStruct->files = files;
    appStruct->fileNameShmMap = NULL;
    appStruct->fileNameShmMapName = NULL;
    appStruct->fileNameShmSize = 0;
    appStruct->fileNameShmCount = 0;

    appStruct->viewSemaphoreName = NULL;
    appStruct->viewSemaphore = NULL;
    appStruct->shmName = NULL;
    appStruct->shmfd = -1;
    appStruct->outputfd = -1;
    appStruct->fileNameShmfd = -1;

    // Create shared memory
    appStruct->shmName = calloc(sizeof(char), MAX_SHARED_MEMORY_NAME_LENGTH);
    snprintf(appStruct->shmName, MAX_SHARED_MEMORY_NAME_LENGTH, SHARED_MEMORY_VIEW_FILE, pid);
    appStruct->shmfd = shm_open(appStruct->shmName, O_CREAT | O_RDWR, READ_AND_WRITE_PERM);
    if (appStruct->shmfd == -1) {
        perror("Could not create shared memory object");
        shutdown(appStruct, ERROR_SHMOPEN_FAIL);
    }

    // Reserve storage for shared memory
    size_t mapSize = sizeof(long) + sizeof(SatStruct) * filesSize; // Long indicates filesqty.
    if (ftruncate(appStruct->shmfd, mapSize) == -1) {
        perror("Could not expand shared memory object");
        shutdown(appStruct, ERROR_FTRUNCATE_FAIL);
    }

    // Map the shared memory
    char *map = mmap(NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, appStruct->shmfd, 0);
    *((long*) map) = filesSize;
    appStruct->satStructs = (SatStruct*) (map + sizeof(long));
    if (appStruct->satStructs == MAP_FAILED) {
        perror("Could not map shared memory");
        shutdown(appStruct, ERROR_MMAP_FAIL);
    }

    // Create shared memory
    appStruct->fileNameShmMapName = calloc(sizeof(char), MAX_SHARED_MEMORY_NAME_LENGTH);
    snprintf(appStruct->fileNameShmMapName, MAX_SHARED_MEMORY_NAME_LENGTH, SHARED_MEMORY_VIEW_FILENAME_FILE, pid);
    appStruct->fileNameShmfd = shm_open(appStruct->fileNameShmMapName, O_CREAT | O_RDWR, READ_AND_WRITE_PERM);
    if (appStruct->fileNameShmfd == -1) {
        perror("Could not create shared memory object");
        shutdown(appStruct, ERROR_SHMOPEN_FAIL);
    }

    // Reserve storage for shared memory
    appStruct->fileNameShmSize = INITAL_FILENAME_SHM_MAP_SIZE;
    if (ftruncate(appStruct->fileNameShmfd, appStruct->fileNameShmSize) == -1) {
        perror("Could not expand shared memory object");
        shutdown(appStruct, ERROR_FTRUNCATE_FAIL);
    }

    // Map the shared memory
    appStruct->fileNameShmMap = (char*) mmap(NULL, appStruct->fileNameShmSize, PROT_READ | PROT_WRITE, MAP_SHARED, appStruct->fileNameShmfd, 0);
    if (appStruct->fileNameShmMap == MAP_FAILED) {
        perror("Could not map shared memory");
        shutdown(appStruct, ERROR_MMAP_FAIL);
    }

    // Create shared semaphore (View)
    appStruct->viewSemaphoreName = calloc(1, sizeof(char) * MAX_SEMAPHORE_NAME_LENGTH);
    snprintf(appStruct->viewSemaphoreName, MAX_SEMAPHORE_NAME_LENGTH, SHARED_SEMAPHORE_VIEW_FILE, pid);
    appStruct->viewSemaphore = sem_open(appStruct->viewSemaphoreName, O_CREAT | O_RDWR, READ_AND_WRITE_PERM, 0);
    if (appStruct->viewSemaphore == SEM_FAILED) {
        perror("Could not create shared semaphore");
        shutdown(appStruct, ERROR_SEMOPEN_FAIL);
    }

    // Create the output file
    appStruct->outputfd = open(OUT_FILE, O_CREAT | O_WRONLY | O_TRUNC, READ_AND_WRITE_PERM);
    if (appStruct->outputfd == -1) {
        perror("Could not create output file");
        shutdown(appStruct, ERROR_FILE_OPEN_FAIL);
    }
}

void initializeSlaves(AppStruct *appStruct) {
    int slavesQuantity = getSlavesQuantity(appStruct->filesSize);
    int slavesQuantityDigits = digits(slavesQuantity);
    int fileAvailableSemaphoreNameLength = strlen(SHARED_SEMAPHORE_SAT_FILE);
    int writePipeNameLength = strlen(SHARED_PIPE_SAT_WRITE_FILE);
    int readPipeNameLength = strlen(SHARED_PIPE_SAT_READ_FILE);

    appStruct->slaveStructs = calloc(slavesQuantity, sizeof(SlaveStruct));
    appStruct->pollfdStructs = calloc(slavesQuantity, sizeof(struct pollfd));
    for (int i = 0; i < slavesQuantity; i++) {
        SlaveStruct *slaveStruct = appStruct->slaveStructs  + i;
        struct pollfd *pollfdStruct = appStruct->pollfdStructs + i;

        slaveStruct->id = i;
        appStruct->slavesQuantity += 1;

        // Create Pipes Name
        slaveStruct->writePipeName = malloc(sizeof(*(slaveStruct->writePipeName)) * (writePipeNameLength + slavesQuantityDigits + 1));
        strcpy(slaveStruct->writePipeName, SHARED_PIPE_SAT_WRITE_FILE);
        sprintf(slaveStruct->writePipeName + writePipeNameLength + slavesQuantityDigits - digits(i), "%d", i);
        for (int j = 0; j < slavesQuantityDigits - digits(i); j++) {
            slaveStruct->writePipeName[writePipeNameLength + j] = '0';
        }
        slaveStruct->readPipeName = malloc(sizeof(*(slaveStruct->readPipeName)) * (readPipeNameLength + slavesQuantityDigits + 1));
        strcpy(slaveStruct->readPipeName, SHARED_PIPE_SAT_READ_FILE);
        sprintf(slaveStruct->readPipeName + readPipeNameLength + slavesQuantityDigits - digits(i), "%d", i);
        for (int j = 0; j < slavesQuantityDigits - digits(i); j++) {
            slaveStruct->readPipeName[readPipeNameLength + j] = '0';
        }
        slaveStruct->fileAvailableSemaphoreName = malloc(sizeof(*(slaveStruct->fileAvailableSemaphoreName)) * (fileAvailableSemaphoreNameLength + slavesQuantityDigits + 1));
        strcpy(slaveStruct->fileAvailableSemaphoreName, SHARED_SEMAPHORE_SAT_FILE);
        sprintf(slaveStruct->fileAvailableSemaphoreName + fileAvailableSemaphoreNameLength + slavesQuantityDigits - digits(i), "%d", i);
        for (int j = 0; j < slavesQuantityDigits - digits(i); j++) {
            slaveStruct->fileAvailableSemaphoreName[fileAvailableSemaphoreNameLength + j] = '0';
        }

        // Create FIFO (named pipe)
        if (mkfifo(slaveStruct->writePipeName, READ_AND_WRITE_PERM) == -1) {
            perror("Could not create named pipe");
            shutdown(appStruct, ERROR_FIFO_CREATION_FAIL);
        }
        // Create FIFO (named pipe)
        if (mkfifo(slaveStruct->readPipeName, READ_AND_WRITE_PERM) == -1) {
            perror("Could not create named pipe");
            shutdown(appStruct, ERROR_FIFO_CREATION_FAIL);
        }

        // Create semaphore
        slaveStruct->fileAvailableSemaphore = sem_open(slaveStruct->fileAvailableSemaphoreName, O_CREAT | O_RDWR, READ_AND_WRITE_PERM, 0);
        if (slaveStruct->fileAvailableSemaphore == SEM_FAILED) {
            perror("Could not create shared semaphore");
            shutdown(appStruct, ERROR_SEMOPEN_FAIL);
        }
        
        int forkResult = fork();
        if (forkResult > 0) {
            // Padre
            // Open FIFO
            slaveStruct->writePipefd = open(slaveStruct->writePipeName, O_WRONLY);
            if (slaveStruct->writePipefd == -1) {
                perror("Could not open named pipe");
                shutdown(appStruct, ERROR_FIFO_OPEN_FAIL);
            }

            // Open FIFO
            slaveStruct->readPipefd = open(slaveStruct->readPipeName, O_RDONLY | O_NONBLOCK);
            if (slaveStruct->readPipefd == -1) {
                perror("Could not open named pipe");
                shutdown(appStruct, ERROR_FIFO_OPEN_FAIL);
            }

            pollfdStruct->fd = slaveStruct->readPipefd;
            pollfdStruct->events = POLLIN;
            sendFile(slaveStruct, appStruct->files[appStruct->filesSent], appStruct->filesSent);
            appStruct->filesSent++;
            sem_post(slaveStruct->fileAvailableSemaphore);
        } else if (forkResult == -1) {
            shutdown(appStruct, ERROR_FIFO_OPEN_FAIL);
        } else {
            // Hijo --> Instanciar cada slave (ver enunciado)
            char *arguments[4] = {slaveStruct->writePipeName, slaveStruct->readPipeName, slaveStruct->fileAvailableSemaphoreName, NULL};
            if (execvp("./src/slave", arguments) == -1) {
                printf("Error slave\n");
                exit(1);
                // Error. TERMINAR
            }
        }
    }
}

void shutdownSlave(SlaveStruct *slaveStruct) {
    // TODO: Race condition????
    if (slaveStruct->writePipefd != -1) {
        terminateSlave(slaveStruct);
        if (slaveStruct->fileAvailableSemaphore != NULL) {
            sem_post(slaveStruct->fileAvailableSemaphore);
        }
        close(slaveStruct->writePipefd);
        remove(slaveStruct->writePipeName);
        slaveStruct->writePipefd = -1;
    }
    if (slaveStruct->fileAvailableSemaphore != NULL) {
        sem_close(slaveStruct->fileAvailableSemaphore);
        slaveStruct->fileAvailableSemaphore = NULL;
    }
    if (slaveStruct->fileAvailableSemaphoreName != NULL) {
        sem_unlink(slaveStruct->fileAvailableSemaphoreName);
        free(slaveStruct->fileAvailableSemaphoreName);
        slaveStruct->fileAvailableSemaphoreName = NULL;
    }
    if (slaveStruct->readPipefd != -1) {
        close(slaveStruct->readPipefd);
        remove(slaveStruct->readPipeName);
        slaveStruct->readPipefd = -1;
    }
    if (slaveStruct->writePipeName != NULL) {
        free(slaveStruct->writePipeName);
        slaveStruct->writePipeName = NULL;
    }
    if (slaveStruct->readPipeName != NULL) {
        free(slaveStruct->readPipeName);
        slaveStruct->readPipeName = NULL;
    }
}

void shutdown(AppStruct *appStruct, int exitCode) {
    if (appStruct->slaveStructs != NULL) {
        for (int i = 0; i < appStruct->slavesQuantity; i++) {
            SlaveStruct *slaveStruct = appStruct->slaveStructs + i;
            shutdownSlave(slaveStruct);
        }

        free(appStruct->slaveStructs);
        if (appStruct->pollfdStructs != NULL) {
            free(appStruct->pollfdStructs);
        }
    }
    if (appStruct->satStructs != NULL) {
        if (appStruct->viewSemaphore != NULL) {
            terminateView(appStruct->satStructs, appStruct->filesSolved, appStruct->viewSemaphore);
        }
        // TODO: RACE CONDITION????
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
