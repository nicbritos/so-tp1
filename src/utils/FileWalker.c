#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include <dirent.h>
#include "FileWalker.h"

#define CHUNK_SIZE 5

typedef struct FileWalkerCDT {
    char **list = NULL;
    char *regex = NULL;
    int size = 0;
    int effectiveSize = 0;
} FileWalkerCDT;

void add(char *s, FileWalkerCDT *FileWalkerCDT);
int matchesRegex(char *s, char *regex);
int processEntry(char *entry, FileWalkerCDT *FileWalkerCDT);


// ----------- PUBLIC -----------
FileWalkerCDT *newFileWalkerADT(char *regex) {
    FileWalkerCDT *FileWalkerCDT = calloc(sizeof(FileWalkerCDT));
    FileWalkerCDT->regex = regex;
    return FileWalkerCDT;
}

void listFilesFileWalker(int size, char **inList, FileWalkerCDT *FileWalkerCDT) {
    if (FileWalkerCDT->list != NULL) {
        freeListFileWalker(FileWalkerCDT);
    }

    FileWalkerCDT->list = calloc(CHUNK_SIZE * sizeof(char*));
    FileWalkerCDT->size = CHUNK_SIZE;

    int result;
    for (int i = 0; i < size; i++) {
        processEntry(inList[i], FileWalkerCDT);
    }

    return list;
}

void freeListFileWalker(FileWalkerCDT *FileWalkerCDT) {
    for (int i = 0; i < getCount(FileWalkerCDT); i++) {
        free(FileWalkerCDT->list[i]);
    }
    free(FileWalkerCDT->list);
    FileWalkerCDT->list = NULL;
    FileWalkerCDT->size = 0;
}

int getCountFileWalker(FileWalkerCDT *FileWalkerCDT) {
    return FileWalkerCDT->effectiveSize;
}


// ----------- PRIVATE -----------
void add(char *s, FileWalkerCDT *FileWalkerCDT) {
    if (FileWalkerCDT->size - getCountFileWalker(FileWalkerCDT) == 0) {
        FileWalkerCDT->size += CHUNK_SIZE;
        FileWalkerCDT->list = realloc(FileWalkerCDT->list, sizeof(FileWalkerCDT->list) * FileWalkerCDT->size);
    }

    FileWalkerCDT->list[getCountFileWalker(FileWalkerCDT)] = s;
    FileWalkerCDT->effectiveSize += 1;
}

int matchesRegex(char *s, char *regex) {
    regex_t regex;
    int result;

    result = regcomp(&regex, regex, 0);
    if (result) {
        return 0;
    }


    result = regexec(&regex, "abc", 0, NULL, 0);
    regfree(&regex);

    return !result;
}

void processEntry(char *entry, FileWalkerCDT *FileWalkerCDT) {
    struct stat statData;

    if (stat(inList[i], &statData) == -1) {
        perror("Error getting file description:");
        freeFileWalker(FileWalkerCDT);
        exit(1);
    }

    // Get only data related to file type
    int result = statData.st_mode & S_IFMT;
    // Only process entry if it's a regular file. 
    // The scan process IS NOT RECURSIVE
    if (result == S_IFREG && matchesRegex(inList[i], FileWalkerCDT->regex)) {
        add(inList[i], FileWalkerCDT);
    }
}
