#ifndef SO_UTILS_H
#define SO_UTILS_H

void printError(char *s);

// Gets all filepaths that match regex expression.
// The list is null terminated and must be freed!
char **getFiles(int size, char **list, char *regex);

#endif
