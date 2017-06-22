#ifndef REBLOOM_H
#define REBLOOM_H

#include "contrib/bloom.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Single link inside a scalable bloom filter.
 */
typedef struct SBLink {
    struct bloom inner;  //< Inner structure
    size_t fillbits;     //< Number of bits currently filled
    struct SBLink *next; //< Prior filter
} SBLink;

/**
 * A chain of one or more bloom filters
 */
typedef struct SBChain {
    SBLink *cur;  //< Current filter
    size_t size;  //< Total number of items in all filters
    double error; //< Desired error ratio
    int fixed;    //< Whether new items can be added to the filter
} SBChain;

/**
 * Create a new chain
 * @param initsize The initial desired capacity of the chain
 * @param error_rate desired maximum error probability
 * @return a new chain. Release with @ref SBChain_Free()
 */
SBChain *SBChain_New(size_t initsize, double error_rate);

/**
 * Free a created chain
 * @param sb the created chain
 */
void SBChain_Free(SBChain *sb);

/**
 * Add an item to the chain
 * @param sb the chain
 * @param data item to add
 * @param len length of item
 * @return 0 if newly added, nonzero if new.
 */
int SBChain_Add(SBChain *sb, const void *data, size_t len);

/**
 * Check if an item was previously seen by the chain
 * @param sb the chain to check
 * @param data buffer to inspect
 * @param len length of buffer
 * @return zero if the item is unknown to the chain, nonzero otherwise
 */
int SBChain_Check(const SBChain *sb, const void *data, size_t len);

#ifdef __cplusplus
}
#endif
#endif