#define _GNU_SOURCE

#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <semaphore.h>
#include <sys/select.h>

#include "utils/slaveStruct.h"
#include "utils/satStruct.h"
#include "utils/commonDef.h"
#include "utils/utils.h"
#include "application.h"

#define SHARED_PIPE_SAT_WRITE_FILE "/tmp/tp1SlavePipeW"
#define SHARED_PIPE_SAT_READ_FILE "/tmp/tp1SlavePipeR"
#define SHARED_SEMAPHORE_SAT_FILE "/tp1SlaveSem"
#define OUT_FILE "./solve_output.txt"

#define FILES_MIN_PERCENTAGE 0.05
#define MAX_FILES_PER_SLAVE 2
#define FILES_PER_SLAVE 10.0
#define INFINITE_POLL -1

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

    // TODO: WAit for slaves to finish
    while (appStruct.filesSolved < appStruct.filesSize) {
        int ready = poll(appStruct.pollfdStructs, appStruct.slavesQuantity, INFINITE_POLL);
        if (ready == -1) {
            perror("Error waiting for slave response");
            shutdown(&appStruct, ERROR_WAITING_FOR_RESPONSE);
        } else if (ready != 0) {
            for (int i = 0; i < appStruct.slavesQuantity; i++) {
                SlaveStruct *slaveStruct = appStruct.slaveStructs + i;
                struct pollfd *pollfdStruct = appStruct.pollfdStructs + i;

                if (pollfdStruct->revents & POLLIN) {
                    pollfdStruct->revents = 0;

                    processInput(slaveStruct->readPipefd, appStruct.satStructs + appStruct.filesSolved, &appStruct, slaveStruct);
                    appStruct.filesSolved++;
                    
                    sem_post(appStruct.viewSemaphore);
                    if (appStruct.filesSent < appStruct.filesSize) {
                        int result = sendFile(slaveStruct, appStruct.files[appStruct.filesSent], appStruct.filesSent);
                        if (result != ERROR_NO) {
                            shutdown(&appStruct, result);
                        }
                        appStruct.filesSent++;
                        sem_post(slaveStruct->fileAvailableSemaphore);
                    } else if (slaveStruct->filesSent == 0) {
                        terminateSlave(slaveStruct);
                    }
                }
            }
        }
    }

    saveFile(appStruct.outputfd, appStruct.filesSolved, appStruct.satStructs);

    for (int i = 0; i < appStruct.slavesQuantity; i++) {
        SlaveStruct *slaveStruct = appStruct.slaveStructs + i;
        terminateSlave(slaveStruct);
        waitpid(-1, NULL, 0);  // Prevent zombie
    }

    shutdown(&appStruct, ERROR_NO);
}

int getMaxSlavesQuantity() {
    int maxFiles = sysconf(_SC_OPEN_MAX);
    // Restamos 10 ya que: 2 corresponden a los fd de los shm,
    // 1 al fd del semaforo del view, 2 al stdin y stdout de
    // este proceso y 5 mas para tener margen.
    // Este numero tambien es seguro de utilizar (en el sentido
    // de que provoca que la funcion devuelva un numero  siempre
    // >= 1) debido a que POSIX define un minimo de 20 fds
    // abiertos a soportar por proceso.
    return (maxFiles - 10) / 3;
}

int getSlavesQuantity(long filesSize) {
    return min(ceil(filesSize / FILES_PER_SLAVE), getMaxSlavesQuantity());
}

int getFilesPerSlaveQuantity(long filesSize) {
    return max(1, min(MAX_FILES_PER_SLAVE, floor(filesSize / (float) getSlavesQuantity(filesSize))));
}

void saveFile(int fd, int count, SatStruct *satStructs) {
    for (int i = 0; i < count; i++) {
        dumpResults(fd, satStructs + i);
    }
}

int sendFile(SlaveStruct *slaveStruct, char *fileName, long fileIndex) {
    int fileNameLength = strlen(fileName);
    int fileIndexDigits;

    if (fileIndex < 0) {
        fileIndexDigits = digits(-fileIndex) + 1;
    } else {
        fileIndexDigits = digits(fileIndex);
    }
    
    char *data = malloc(sizeof(*data) * (fileNameLength + 1 + fileIndexDigits + 2));
    if (data == NULL) {
        return ERROR_ALLOC_MEMORY;
    }
    sprintf(data, "%s\n%ld\n", fileName, fileIndex);
    write(slaveStruct->writePipefd, data, fileNameLength + fileIndexDigits + 3);
    free(data);
    slaveStruct->filesSent += 1;

    return ERROR_NO;
}

void processInput(int fd, SatStruct *satStruct, AppStruct *appStruct, SlaveStruct *slaveStruct) {
    long fileIndex;
    char *data = readFromFile(fd);
    if (data == NULL)
        return;

    sscanf(data, "%d\n%d\n%f\n%d\n%ld\n", &(satStruct->variables), &(satStruct->clauses), &(satStruct->processingTime), &(satStruct->isSat), &fileIndex);
    free(data);

    satStruct->fileName = appStruct->files[fileIndex];
    satStruct->fileNameLength = strlen(satStruct->fileName);
    satStruct->processedBySlaveID = slaveStruct->id;
    slaveStruct->filesSent -= 1;

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
    if (!slaveStruct->terminated) {
        sendFile(slaveStruct, "\n", -1);
        if (slaveStruct->fileAvailableSemaphore != NULL) {
            sem_post(slaveStruct->fileAvailableSemaphore);
        }

        slaveStruct->terminated = 1;
    }
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
    if (appStruct->shmName == NULL) {
        printError("Error allocating memory for saving shared memory name");
        shutdown(appStruct, ERROR_ALLOC_MEMORY);
    }
    snprintf(appStruct->shmName, MAX_SHARED_MEMORY_NAME_LENGTH, SHARED_MEMORY_VIEW_FILE, pid);
    appStruct->shmfd = shm_open(appStruct->shmName, O_CREAT | O_RDWR, READ_AND_WRITE_PERM);
    if (appStruct->shmfd == -1) {
        perror("Could not create shared memory object");
        shutdown(appStruct, ERROR_SHMOPEN_FAIL);
    }

    // Reserve storage for shared memory
    appStruct->satStructsSize = sizeof(long) + sizeof(SatStruct) * filesSize; // Long indicates filesqty.
    if (ftruncate(appStruct->shmfd, appStruct->satStructsSize) == -1) {
        perror("Could not expand shared memory object");
        shutdown(appStruct, ERROR_FTRUNCATE_FAIL);
    }

    // Map the shared memory
    char *map = mmap(NULL, appStruct->satStructsSize, PROT_READ | PROT_WRITE, MAP_SHARED, appStruct->shmfd, 0);
    *((long*) map) = filesSize;
    appStruct->satStructs = (SatStruct*) (map + sizeof(long));
    if (appStruct->satStructs == MAP_FAILED) {
        perror("Could not map shared memory");
        shutdown(appStruct, ERROR_MMAP_FAIL);
    }

    // Create shared memory
    appStruct->fileNameShmMapName = calloc(sizeof(char), MAX_SHARED_MEMORY_NAME_LENGTH);
    if (appStruct->fileNameShmMapName == NULL) {
        printError("Error allocating memory for saving fileName shared memory name");
        shutdown(appStruct, ERROR_ALLOC_MEMORY);
    }
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
    appStruct->viewSemaphoreName = calloc(sizeof(char), MAX_SEMAPHORE_NAME_LENGTH);
    if (appStruct->viewSemaphoreName == NULL) {
        printError("Error allocating memory for saving view's semaphore name");
        shutdown(appStruct, ERROR_ALLOC_MEMORY);
    }
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
    int filesPerSlave = getFilesPerSlaveQuantity(appStruct->filesSize);
    int slavesQuantityDigits = digits(slavesQuantity);
    int fileAvailableSemaphoreNameLength = strlen(SHARED_SEMAPHORE_SAT_FILE);
    int writePipeNameLength = strlen(SHARED_PIPE_SAT_WRITE_FILE);
    int readPipeNameLength = strlen(SHARED_PIPE_SAT_READ_FILE);

    appStruct->slaveStructs = calloc(slavesQuantity, sizeof(SlaveStruct));
    if (appStruct->slaveStructs == NULL) {
        printError("Error allocating memory for saving slaveStructs");
        shutdown(appStruct, ERROR_ALLOC_MEMORY);
    }
    appStruct->pollfdStructs = calloc(slavesQuantity, sizeof(struct pollfd));
    if (appStruct->pollfdStructs == NULL) {
        printError("Error allocating memory for saving slaveReadFds");
        shutdown(appStruct, ERROR_ALLOC_MEMORY);
    }
    appStruct->filesSent = 0;
    for (int i = 0; i < slavesQuantity; i++) {
        SlaveStruct *slaveStruct = appStruct->slaveStructs  + i;
        struct pollfd *pollfdStruct = appStruct->pollfdStructs + i;

        slaveStruct->id = i;
        appStruct->slavesQuantity += 1;
        slaveStruct->filesSent = 0;
        slaveStruct->terminated = 0;

        // Create Pipes Name
        slaveStruct->writePipeName = malloc(sizeof(*(slaveStruct->writePipeName)) * (writePipeNameLength + slavesQuantityDigits + 1));
        if (slaveStruct->writePipeName == NULL) {
            printError("Error allocating memory for saving slave's write pipe name");
            shutdown(appStruct, ERROR_ALLOC_MEMORY);
        }
        strcpy(slaveStruct->writePipeName, SHARED_PIPE_SAT_WRITE_FILE);
        sprintf(slaveStruct->writePipeName + writePipeNameLength + slavesQuantityDigits - digits(i), "%d", i);
        for (int j = 0; j < slavesQuantityDigits - digits(i); j++) {
            slaveStruct->writePipeName[writePipeNameLength + j] = '0';
        }
        slaveStruct->readPipeName = malloc(sizeof(*(slaveStruct->readPipeName)) * (readPipeNameLength + slavesQuantityDigits + 1));
        if (slaveStruct->readPipeName == NULL) {
            printError("Error allocating memory for saving slave's read pipe name");
            shutdown(appStruct, ERROR_ALLOC_MEMORY);
        }
        strcpy(slaveStruct->readPipeName, SHARED_PIPE_SAT_READ_FILE);
        sprintf(slaveStruct->readPipeName + readPipeNameLength + slavesQuantityDigits - digits(i), "%d", i);
        for (int j = 0; j < slavesQuantityDigits - digits(i); j++) {
            slaveStruct->readPipeName[readPipeNameLength + j] = '0';
        }
        slaveStruct->fileAvailableSemaphoreName = malloc(sizeof(*(slaveStruct->fileAvailableSemaphoreName)) * (fileAvailableSemaphoreNameLength + slavesQuantityDigits + 1));
        if (slaveStruct->fileAvailableSemaphoreName == NULL) {
            printError("Error allocating memory for saving slave's semaphore name");
            shutdown(appStruct, ERROR_ALLOC_MEMORY);
        }
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

            for (int j = 0; j < filesPerSlave; j++) {
                sendFile(slaveStruct, appStruct->files[appStruct->filesSent], appStruct->filesSent);
                appStruct->filesSent++;
                sem_post(slaveStruct->fileAvailableSemaphore);
            }
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
    if (slaveStruct->writePipefd != -1) {
        terminateSlave(slaveStruct);
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
        appStruct->slaveStructs = NULL;
    } 
    if (appStruct->pollfdStructs != NULL) {
        free(appStruct->pollfdStructs);
        appStruct->pollfdStructs = NULL;
    }
    if (appStruct->satStructs != NULL) {
        if (appStruct->viewSemaphore != NULL) {
            terminateView(appStruct->satStructs, appStruct->filesSolved, appStruct->viewSemaphore);
        }
        munmap(appStruct->satStructs, appStruct->satStructsSize);
        appStruct->satStructs = NULL;
    }
    if (appStruct->viewSemaphore != NULL) {
        sem_close(appStruct->viewSemaphore);
        appStruct->viewSemaphore = NULL;
    }
    if (appStruct->viewSemaphoreName != NULL) {
        sem_unlink(appStruct->viewSemaphoreName);
        free(appStruct->viewSemaphoreName);
        appStruct->viewSemaphoreName = NULL;
    }
    if (appStruct->outputfd != -1) {
        close(appStruct->outputfd);
        appStruct->outputfd = -1;
    }
    if (appStruct->shmfd != -1) {
        close(appStruct->shmfd);
        appStruct->shmfd = -1;
    }
    if (appStruct->fileNameShmfd != -1) {
        close(appStruct->fileNameShmfd);
        appStruct->fileNameShmfd = -1;
    }
    if (appStruct->shmName != NULL) {
        shm_unlink(appStruct->shmName);
        free(appStruct->shmName);
        appStruct->shmName = NULL;
    }
    if (appStruct->fileNameShmMapName != NULL) {
        shm_unlink(appStruct->fileNameShmMapName);
        free(appStruct->fileNameShmMapName);
        appStruct->fileNameShmMapName = NULL;
    }

    exit(exitCode);
}
