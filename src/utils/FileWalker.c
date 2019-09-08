#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include <dirent.h>
#include "FileWalker.h"

#define CHUNK_SIZE 5

typedef struct FileWalker {
    char **list = NULL;
    char *regex = NULL;
    int size = 0;
    int effectiveSize = 0;
} FileWalkerCDT;

void add(char *s, FileWalker *fileWalker);
int matchesRegex(char *s, char *regex);
int processEntry(char *entry, FileWalker *fileWalker);


// ----------- PUBLIC -----------
FileWalker *newFileWalkerADT(char *regex) {
    FileWalker *fileWalker = calloc(sizeof(FileWalker));
    fileWalker->regex = regex;
    return fileWalker;
}

void listFilesFileWalker(int size, char **inList, FileWalker *fileWalker) {
    if (fileWalker->list != NULL) {
        freeListFileWalker(fileWalker);
    }

    fileWalker->list = calloc(CHUNK_SIZE * sizeof(char*));
    fileWalker->size = CHUNK_SIZE;

    int result;
    for (int i = 0; i < size; i++) {
        processEntry(inList[i], fileWalker);
    }

    return list;
}

void freeListFileWalker(FileWalker *fileWalker) {
    for (int i = 0; i < getCount(fileWalker); i++) {
        free(fileWalker->list[i]);
    }
    free(fileWalker->list);
    fileWalker->list = NULL;
    fileWalker->size = 0;
}

int getCountFileWalker(FileWalker *fileWalker) {
    return fileWalker->effectiveSize;
}


// ----------- PRIVATE -----------
void add(char *s, FileWalker *fileWalker) {
    if (fileWalker->size - getCountFileWalker(fileWalker) == 0) {
        fileWalker->size += CHUNK_SIZE;
        fileWalker->list = realloc(fileWalker->list, sizeof(fileWalker->list) * fileWalker->size);
    }

    fileWalker->list[getCountFileWalker(fileWalker)] = s;
    fileWalker->effectiveSize += 1;
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

void processEntry(char *entry, FileWalker *fileWalker) {
    struct stat statData;

    if (stat(inList[i], &statData) == -1) {
        perror("Error getting file description:");
        freeFileWalker(fileWalker);
        exit(1);
    }

    // Get only data related to file type
    int result = statData.st_mode & S_IFMT;
    // Only process entry if it's a regular file. 
    // The scan process IS NOT RECURSIVE
    if (result == S_IFREG && matchesRegex(inList[i], fileWalker->regex)) {
        add(inList[i], fileWalker);
    }
}
