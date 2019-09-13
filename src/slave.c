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

#define READ_AND_WRITE_PERM 0666

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
    


    exit(ERROR_NO);
}
