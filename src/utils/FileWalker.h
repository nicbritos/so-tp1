#ifndef SO_FILE_WALKER_H
#define SO_FILE_WALKER_H

typedef struct FileWalkerCDT *FileWalkerADT;

FileWalkerADT newFileWalkerADT(char *regex);
void listFilesFileWalker(int size, char **inList, FileWalkerADT fileWalkerADT);
int getCountFileWalker(FileWalkerADT fileWalkerADT);
void freeListFileWalker(FileWalkerADT fileWalkerADT);

#endif
