#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "utils/utils.h"
#include "utils/FileWalker.h"

#define SHARED_MEMORY_NAME "/pSharedMem"
#define SHARED_SEMAPHORE_NAME "/pSharedSem"

#define REGEX_MATCH ".*"

int main(int argc, char **argv) {
    if (argc < 2) {
        printError("You need to provide at least one CNF file.");
        return -1;
    }

    FileWalkerADT fileWalkerADT = newFileWalkerADT(REGEX_MATCH);
    listFilesFileWalker(argc - 1, argv + 1, fileWalkerADT);
    printf("Result: %d\n", getCountFileWalker(fileWalkerADT));

    // int fd = shm_open(SHARED_MEMORY_NAME, O_RDWR | O_CREAT, 777);
    // if (fd == -1) {
    //     perror("Could not create shared memory object: ");
    //     return -1;
    // }
    // if (ftruncate(fd, sizeof(sum)) == -1) {
    //     perror("Could not expand shared memory object: ");
    //     return -1;
    // }

    // void *map = mmap(NULL, sizeof(sum), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // if (map == NULL) {
    //     perror("Could not map shared memory: ");
    //     return -1;
    // }

    // sem_t *sem = sem_open(SHARED_SEMAPHORE_NAME, O_CREAT | O_RDWR);
    // if (sem == SEM_FAILED) {
    //     perror("Could not create shared semaphore: ");
    //     return -1;
    // }

    // pid_t children[processes];
    // *((long*) sem) = 0;
    // int j = 0;
    // for (int i = 0; i < processes; i++) {
    //     int forkResult = fork();
    //     if (forkResult > 0) {
    //         // Padre
    //         children[j++] = forkResult;
    //     } else if (forkResult == -1) {
    //         // error
    //     } else {
    //         // Hijo
    //         char *arguments[4] = {SHARED_MEMORY_NAME, SHARED_SEMAPHORE_NAME, argv[2], NULL};
    //         execvp("./sum.out", arguments);
    //     }
    // }

    // for (int i = 0; i < j; i++) {
    //     waitpid(children[i], NULL, 0);
    // }

    // printf("Number is: %ld\n", (*(long*) map));

    // close(fd);
    // shm_unlink(SHARED_MEMORY_NAME);
    // sem_close(sem);
    // sem_unlink(SHARED_SEMAPHORE_NAME);

    exit(0);
}
