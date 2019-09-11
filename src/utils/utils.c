#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utils.h"


void printError(char *s) {
    fprintf(stderr, "%s\n", s);
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

int min(int a, int b){
	return (a<b)?a:b;
}