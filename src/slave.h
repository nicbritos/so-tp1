#ifndef _SO_VIEW_H_
#define _SO_VIEW_H_

char *readFilepath(int pipefd, char *oldFilepath);
void processFile(int pipefd, char *filepath);
void sendSolution(int pipefd, char *solution);

#endif
