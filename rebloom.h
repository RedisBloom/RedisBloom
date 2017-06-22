#ifndef REBLOOM_H
#define REBLOOM_H

#include "deps/bloom.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Single link inside a scalable bloom filter.
 */
typedef struct sbLink {
    struct bloom inner;  //< Inner structure
    size_t fillbits;     //< Number of bits currently filled
    struct sbLink *next; //< Prior filter
} sbLink;

/**
 * A chain of one or more bloom filters
 */
typedef struct sbChain {
    sbLink *cur;          //< Current filter
    size_t total_entries; //< Total number of items in all filters
    double error;         //< Desired error ratio
    int is_fixed;         //< Whether new items can be added to the filter
} sbChain;

/**
 * Create a new chain
 * @param initsize The initial desired capacity of the chain
 * @param error_rate desired maximum error probability
 * @return a new chain. Release with @ref sbFreeChain()
 */
sbChain *sbCreateChain(size_t initsize, double error_rate);

/**
 * Free a created chain
 * @param sb the created chain
 */
static void sbFreeChain(sbChain *sb);

/**
 * Add an item to the chain
 * @param sb the chain
 * @param data item to add
 * @param len length of item
 * @return nonzero if item had previously existed, 0 if newly added.
 */
int sbAdd(sbChain *sb, const void *data, size_t len);

/**
 * Check if an item was previously seen by the chain
 * @param sb the chain to check
 * @param data buffer to inspect
 * @param len length of buffer
 * @return zero if the item is unknown to the chain, nonzero otherwise
 */
int sbCheck(const sbChain *sb, const void *data, size_t len);

#ifdef __cplusplus
}
#endif
#endif