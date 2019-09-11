#ifndef _SAT_STRUCT_H_
#define _SAT_STRUCT_H_

typedef struct SatStruct {
	char *filename;
	int clauses;
	int variables;
	int isSat;
	long processingTime;
	long processedBySlaveID;
} SatStruct;

#endif
