#ifndef _SAT_STRUCT_H_
#define _SAT_STRUCT_H_

typedef struct SatStruct {
	char *fileName;
	int clauses;
	int variables;
	int isSat;
	float processingTime;
	long processedBySlaveID;
} SatStruct;

#endif
