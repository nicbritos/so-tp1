#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commonDef.h"
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
         "Processing time: %f\n"
         "Processed by Slave ID: %ld\n"
         "--------------------------\n", 
        satStruct->fileName,
        satStruct->clauses,
        satStruct->variables,
        satStruct->isSat == 0 ? "UNSAT" : "SAT",
        satStruct->processingTime,
        satStruct->processedBySlaveID);
}

long min(long a, long b){
	return (a<b)?a:b;
}

long max(long a, long b) {
    return a > b ? a : b;
}

int digits(long n){
    if(n == 0)
        return 1;
    int ans = 0;
    if (n < 0) {
        ans++;
        n = -n;
    }
    while(n > 0){
        ans++;
        n/=10; 
    }
    return ans;
}

char *readFromFile(int fd) {
    int bytesRead = 0, result = 0;

    char *out = malloc(sizeof(*out) * CHUNK);
    if (out == NULL) {
        return NULL;
    }

    int size = CHUNK;
    while ((result = read(fd, out + bytesRead, 1)) > 0 && out[bytesRead] != '\0') {
        bytesRead++;

        if ((bytesRead % CHUNK) == 0) {
            char *aux = realloc(out, sizeof(*out) * (CHUNK + bytesRead));
            if (aux == NULL) {
                free(out);
                return NULL;
            }
            out = aux;
            size += CHUNK;
        }
    }

    if (bytesRead > 0 || result > 0) {
        size = bytesRead + 1;
        if ((size % CHUNK)!= 0) {
            char *aux = realloc(out, sizeof(*out) * size);
            if (aux == NULL) {
                free(out);
                return NULL;
            }
            out = aux;
        }
        return out;
    } else {
        free(out);
        return NULL;
    }
}
