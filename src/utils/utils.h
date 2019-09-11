#ifndef _SO_UTILS_H_
#define _SO_UTILS_H_

void printError(char *s);

// Gets all filepaths that match regex expression.
// The list is null terminated and must be freed!
char **getFiles(int size, char **list, char *regex);

#endif
