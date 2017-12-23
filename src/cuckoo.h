#ifndef CUCKOO_H
#define CUCKOO_H

#include <stdint.h>
#include <stdlib.h>

typedef uint8_t CuckooFingerprint;
typedef uint32_t CuckooHash;
typedef uint8_t CuckooBucket[4];

#define CUCKOO_BKTSIZE 4
#define CUCKOO_NULLFP 0

typedef struct {
    size_t numBuckets;
    size_t numItems;
    size_t numFilters;
    size_t numDeletes;
    CuckooBucket **filters;
} CuckooFilter;

#define CUCKOO_GEN_HASH(s, n) murmurhash2(s, n, 0)

typedef struct {
    size_t i1;
    size_t i2;
    CuckooFingerprint fp;
} CuckooKey;

typedef enum {
    CuckooInsert_Inserted = 1,
    CuckooInsert_Exists = 0,
    CuckooInsert_NoSpace = -1
} CuckooInsertStatus;

int CuckooFilter_Init(CuckooFilter *filter, size_t n2);
void CuckooFilter_Free(CuckooFilter *filter);
CuckooInsertStatus CuckooFilter_InsertUnique(CuckooFilter *filter, CuckooHash hash);
CuckooInsertStatus CuckooFilter_Insert(CuckooFilter *filter, CuckooHash hash);
int CuckooFilter_Delete(CuckooFilter *filter, CuckooHash hash);
int CuckooFilter_Check(const CuckooFilter *filter, CuckooHash hash);

#endif