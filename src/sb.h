#ifndef REBLOOM_H
#define REBLOOM_H

#include "contrib/bloom.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Single link inside a scalable bloom filter */
typedef struct SBLink {
    struct bloom inner; //< Inner structure
    size_t size;        // < Number of items in the link
} SBLink;

/** A chain of one or more bloom filters */
typedef struct SBChain {
    SBLink *filters;  //< Current filter
    size_t size;      //< Total number of items in all filters
    size_t nfilters;  //< Number of links in chain
    unsigned options; //< Options passed directly to bloom_init
} SBChain;

/**
 * Create a new chain
 * initsize: The initial desired capacity of the chain
 * error_rate: desired maximum error probability.
 * options: Options passed to bloom_init.
 *
 * Free with SBChain_Free when done.
 */
SBChain *SB_NewChain(size_t initsize, double error_rate, unsigned options);

/** Free a created chain */
void SBChain_Free(SBChain *sb);

/**
 * Add an item to the chain
 * Returns 0 if newly added, nonzero if new.
 */
int SBChain_Add(SBChain *sb, const void *data, size_t len);

/**
 * Check if an item was previously seen by the chain
 * Return 0 if the item is unknown to the chain, nonzero otherwise
 */
int SBChain_Check(const SBChain *sb, const void *data, size_t len);

#ifdef __cplusplus
}
#endif
#endif