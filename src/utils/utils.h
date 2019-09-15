#ifndef _SO_UTILS_H_
#define _SO_UTILS_H_

#include <semaphore.h>
#include "satStruct.h"

void printError(char *s);

void dumpResults(int fd, SatStruct *satStruct);

int min(int a, int b);

int digits(long n);

char *readFromFile(int fd);

#endif
