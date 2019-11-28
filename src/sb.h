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
    unsigned growth;
} SBChain;

/**
 * Create a new chain
 * initsize: The initial desired capacity of the chain
 * error_rate: desired maximum error probability.
 * options: Options passed to bloom_init.
 *
 * Free with SBChain_Free when done.
 */
SBChain *SB_NewChain(size_t initsize, double error_rate, unsigned options, unsigned growth);

/**
 * Create a new chain from a 'template'. This template will copy an existing
 * chain, but not its internal data - which is reset from scratch. This is
 * used when 'migrating' filters
 */
SBChain *SB_NewChainFromTemplate(const SBChain *template);

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

/**
 * Get an encoded header. This is the first step to serializing a bloom filter.
 * The length of the header will be written to in hdrlen.
 *
 * The chunk should be freed with SB_FreeEncodedHeader.
 */
char *SBChain_GetEncodedHeader(const SBChain *sb, size_t *hdrlen);
void SB_FreeEncodedHeader(char *s);

#define SB_CHUNKITER_INIT 1
#define SB_CHUNKITER_DONE 0
/**
 * Get an encoded filter chunk. This filter should be called in a loop until it
 * returns NULL. It will return incremental chunks based on the value of `curIter`.
 *
 * Prior to calling this function, the target of curIter should be initialized to
 * SB_CHUNKITER_INIT. Once there are no more chunks left to serialize, NULL will
 * be returned and curIter will be set to SB_CHUNKITER_DONE.
 *
 * The `len` pointer indicates the length of the returned buffer.
 * maxChunkSize indicates the largest chunk size to be returned, ensuring that
 * len <= maxChunkSize.
 */
const char *SBChain_GetEncodedChunk(const SBChain *sb, long long *curIter, size_t *len,
                                    size_t maxChunkSize);

/**
 * Creates a new chain from the encoded parameters returned by SBChain_GetEncodedHeader.
 * This function will return NULL if the header is corrupt or in a format not understood
 * by this version of rebloom. In this case, the description is found in the
 * errmsg pointer.
 */
SBChain *SB_NewChainFromHeader(const char *buf, size_t bufLen, const char **errmsg);

/**
 * Incrementally load the bloom filter with chunks returned from GetEncodedChunk.
 * This function returns 0 on success, and nonzero on failure - in which case errmsg
 * is populated.
 *
 * The (iter,buf,bufLen) arguments are equivalent to the returned values in the
 * GetEncodedChunk function.
 */
int SBChain_LoadEncodedChunk(SBChain *sb, long long iter, const char *buf, size_t bufLen,
                             const char **errmsg);
#ifdef __cplusplus
}
#endif
#endif