#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utils.h"

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
