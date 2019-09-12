#ifndef _SO_VIEW_H_
#define _SO_VIEW_H_

void printResults(SatStruct *satStruct);
SatStruct* getNextSatStruct(SatStruct *oldMap, int sharedMemoryfd, int count);

#endif
