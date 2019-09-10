#ifndef _SAT_STRUCT_H
#define _SAT_STRUCT_H

typedef struct SatStruct {
	char *filename;
	int clauses;
	int variables;
	int isSat;
	long processingTime;
	long processedBySlaveID;
} SatStruct;

#endif
