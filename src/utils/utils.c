#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utils.h"

#define CHUNK 5

void printError(char *s) {
    fprintf(stderr, "%s\n", s);
}

void dumpResults(int fd, SatStruct *satStruct) {
    dprintf(fd, 
        "Filename: %s\n"
         "Clauses: %d\n"
         "Variables: %d\n"
         "Satisfiable: %s\n"
         "Processing time: %ld\n"
         "Processed by Slave ID: %ld\n"
         "--------------------------\n", 
        satStruct->filename,
        satStruct->clauses,
        satStruct->variables,
        satStruct->isSat == 0 ? "UNSAT" : "SAT",
        satStruct->processingTime,
        satStruct->processedBySlaveID);
}

void closeSharedMemory(int fd) {
    close(fd);
}

void closeSemaphore(sem_t *sem) {
    sem_close(sem);
}

void closeMasterSharedMemory(int fd, char *name) {
    closeSharedMemory(fd);
    shm_unlink(name);
}

void closeMasterSemaphore(sem_t *sem, char *name) {
    closeSemaphore(sem);
    sem_unlink(name);
}

void closePipe(int fd) {
    close(fd);
}

void unmapSharedMemory(void *addr, size_t length) {
	munmap(addr, length);
}

int min(int a, int b){
	return (a<b)?a:b;
}

int digits(long n){
    if(n == 0)
        return 1;
    int ans = 0;
    while(n > 0){
        ans++;
        n/=10; 
    }
    return ans;
}

char *readFromFile(int fd) {
    int bytesRead = 0, bytesReadThisRound = 0;
    char *out = malloc(sizeof(*out) * CHUNK);
    while ((bytesReadThisRound = read(fd, out + bytesRead, CHUNK)) == CHUNK) {
        bytesRead += CHUNK;
        out = realloc(out, sizeof(*out) * (CHUNK + bytesRead));
    }
    bytesRead += bytesReadThisRound;
    if (bytesRead == 0) {
        free(out);
        return NULL;
    }
    out = realloc(out, sizeof(*out) * (bytesRead + 1)); //Espacio justo para el string y \0
    out[bytesRead] = '\0'; //NULL terminated
    return out;
}
