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

#define MAXSIZE 80
 
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

	char * filePath = "cnf/uuf100-0990.cnf"; //VA A SER VARIABLE
	char * command = malloc(MAXSIZE + strlen(filePath) + 1);
	sprintf(command, "%s %s | %s","minisat", filePath, "egrep \"Number of|CPU|SAT\" | egrep -o \"[0-9]+\\.?[0-9]*|(UN)?SAT\"");
    FILE * output = popen(command, "r");
    int vars, clauses, cpuTime, isSat;
    char sat[6]; //UNSAT + \0
    
    fscanf(output, "%d\n%d\n%d\n%s\n", &vars, &clauses, &cpuTime, sat);
    isSat = (sat[0] == 'U')?0:(sat[0] == 'S')?1:-1;
    

    printf("%d\n%d\n%d\n%d\n", vars, clauses, cpuTime, isSat);

    free(command);
    pclose(output);
    exit(ERROR_NO);
}
