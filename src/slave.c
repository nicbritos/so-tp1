#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "slave.h"
#include "utils/utils.h"
#include "utils/satStruct.h"

#define SHARED_PIPE_SAT_NPIPE_FILE "/tmp/tp1%lu"

#define READ_AND_WRITE_PERM 0666

#define MAXSIZE 80
 
#define ERROR_NO 0
#define ERROR_NOT_ENOUGH_ARGUMENTS -1
#define ERROR_SHMOPEN_FAIL -2
#define ERROR_FTRUNCATE_FAIL -3
#define ERROR_MMAP_FAIL -4
#define ERROR_SEMOPEN_FAIL -5
#define ERROR_FIFO_CREATION_FAIL -6
#define ERROR_FIFO_OPEN_FAIL -7
#define ERROR_FILE_OPEN_FAIL -8

int main(int argc, char **argv) {
    if (argc < 3) {
        printError("Not enough arguments.");
        exit(ERROR_NOT_ENOUGH_ARGUMENTS);
    }

    char *readPipeName = argv[0];
    char *writePipeName = argv[1];
    char *semaphoreName = argv[2];

    // Tiene que estar en el mismo orden que en el Application!!!!
    int readPipefd = open(readPipeName, O_RDONLY);
    if (readPipefd == -1) {
        perror("Could not open named pipe: ");
        exit(ERROR_FIFO_OPEN_FAIL);
    }
    int writePipefd = open(writePipeName, O_WRONLY);
    if (writePipefd == -1) {
        perror("Could not open named pipe: ");
        exit(ERROR_FIFO_OPEN_FAIL);
    }
    sem_t *semaphore = sem_open(semaphoreName, O_RDWR);
    if (semaphore == SEM_FAILED) {
        perror("Could not open shared semaphore");
        exit(ERROR_SEMOPEN_FAIL);
    }

    char *filepath = NULL;
    long fileId;
    printf("Slave\n");
    while ((filepath = readFilepath(readPipefd, filepath, semaphore, &fileId)) != NULL) {
        printf("Slave: filepath: %s\n", filepath);
        processFile(writePipefd, filepath, fileId);
    }

    printf("Slave: Exit\n");
    close(writePipefd);
    close(readPipefd);
    exit(ERROR_NO);
}

char *readFilepath(int pipefd, char *oldFilepath, sem_t *semaphore, long *fileId) {
    if (oldFilepath != NULL) {
        free(oldFilepath);
    }

    sem_wait(semaphore);
    char *data = readFromFile(pipefd);
    char *separatorPointer = strchr(data, '\n');
    sscanf(separatorPointer + 1, "%ld", fileId);
    *separatorPointer = '\0';

    return data;
}

void processFile(int pipefd, char *filepath, long fileId) {
    char *command = malloc(MAXSIZE + strlen(filepath) + 1);
    sprintf(command, "%s %s | %s","minisat", filepath, "egrep \"Number of|CPU|SAT\" | egrep -o \"[0-9]+\\.?[0-9]*|(UN)?SAT\"");
    FILE *output = popen(command, "r");
    printf("Slave: command: %s\n", command);
    free(command);

    int vars, clauses, isSat;
    float cpuTime;
    char sat[6]; //UNSAT + \0
    fscanf(output, "%d\n%d\n%f\n%s\n", &vars, &clauses, &cpuTime, sat);
    isSat = (sat[0] == 'U')?0:(sat[0] == 'S')?1:-1;
    pclose(output);

    char *solutionData = malloc(sizeof(*solutionData) * (digits(vars) + 1 + digits(clauses) + 1 + digits(cpuTime) + 1 + digits(isSat) + 1 + digits(fileId) + 2));
    sprintf(solutionData, "%d\n%d\n%f\n%d\n%ld\n", vars, clauses, cpuTime, isSat, fileId);
    sendSolution(pipefd, solutionData);
    printf("Slave: solution: %s\n", solutionData);
    free(solutionData);
}

void sendSolution(int pipefd, char *solution) {
    write(pipefd, solution, strlen(solution));
}
