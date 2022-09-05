#include "sb.h"
#define REDISMODULE_MAIN
#include "redismodule.h"
#define BLOOM_CALLOC RedisModule_Calloc
#define BLOOM_FREE RedisModule_Free
#include "contrib/bloom.c"
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Core                                                                     ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define ERROR_TIGHTENING_RATIO 0.5
#define CUR_FILTER(sb) ((sb)->filters + ((sb)->nfilters - 1))

static int SBChain_AddLink(SBChain *chain, uint64_t size, double error_rate) {
    if (!chain->filters) {
        chain->filters = RedisModule_Calloc(1, sizeof(*chain->filters));
    } else {
        chain->filters =
            RedisModule_Realloc(chain->filters, sizeof(*chain->filters) * (chain->nfilters + 1));
    }

    SBLink *newlink = chain->filters + chain->nfilters;
    newlink->size = 0;
    chain->nfilters++;
    return bloom_init(&newlink->inner, size, error_rate, chain->options);
}

void SBChain_Free(SBChain *sb) {
    for (size_t ii = 0; ii < sb->nfilters; ++ii) {
        bloom_free(&sb->filters[ii].inner);
    }
    RedisModule_Free(sb->filters);
    RedisModule_Free(sb);
}

static int SBChain_AddToLink(SBLink *lb, bloom_hashval hash) {
    if (!bloom_add_h(&lb->inner, hash)) {
        // Element not previously present?
        lb->size++;
        return 1;
    } else {
        return 0;
    }
}

static bloom_hashval SBChain_GetHash(const SBChain *chain, const void *buf, size_t len) {
    if (chain->options & BLOOM_OPT_FORCE64) {
        return bloom_calc_hash64(buf, len);
    } else {
        return bloom_calc_hash(buf, len);
    }
}

int SBChain_Add(SBChain *sb, const void *data, size_t len) {
    // Does it already exist?
    bloom_hashval h = SBChain_GetHash(sb, data, len);
    for (int ii = sb->nfilters - 1; ii >= 0; --ii) {
        if (bloom_check_h(&sb->filters[ii].inner, h)) {
            return 0;
        }
    }

    // Determine if we need to add more items?
    SBLink *cur = CUR_FILTER(sb);
    if (cur->size >= cur->inner.entries) {
        if (sb->options & BLOOM_OPT_NO_SCALING) {
            return -2;
        }
        double error = cur->inner.error * ERROR_TIGHTENING_RATIO;
        if (SBChain_AddLink(sb, cur->inner.entries * (size_t)sb->growth, error) != 0) {
            return -1;
        }
        cur = CUR_FILTER(sb);
    }

    int rv = SBChain_AddToLink(cur, h);
    if (rv) {
        sb->size++;
    }
    return rv;
}

int SBChain_Check(const SBChain *sb, const void *data, size_t len) {
    bloom_hashval hv = SBChain_GetHash(sb, data, len);
    for (int ii = sb->nfilters - 1; ii >= 0; --ii) {
        if (bloom_check_h(&sb->filters[ii].inner, hv)) {
            return 1;
        }
    }
    return 0;
}

SBChain *SB_NewChain(uint64_t initsize, double error_rate, unsigned options, unsigned growth) {
    if (initsize == 0 || error_rate == 0 || error_rate >= 1) {
        return NULL;
    }
    SBChain *sb = RedisModule_Calloc(1, sizeof(*sb));
    sb->growth = growth;
    sb->options = options;
    double tightening = (options & BLOOM_OPT_NO_SCALING) ? 1 : ERROR_TIGHTENING_RATIO;
    if (SBChain_AddLink(sb, initsize, error_rate * tightening) != 0) {
        SBChain_Free(sb);
        sb = NULL;
    }
    return sb;
}

typedef struct __attribute__((packed)) {
    uint64_t bytes;
    uint64_t bits;
    uint64_t size;
    double error;
    double bpe;
    uint32_t hashes;
    uint64_t entries;
    uint8_t n2;
} dumpedChainLink;

// X-Macro uses to convert between encoded and decoded SBLink
#define X_ENCODED_LINK(X, enc, link)                                                               \
    X((enc)->bytes, (link)->inner.bytes)                                                           \
    X((enc)->bits, (link)->inner.bits)                                                             \
    X((enc)->size, (link)->size)                                                                   \
    X((enc)->error, (link)->inner.error)                                                           \
    X((enc)->hashes, (link)->inner.hashes)                                                         \
    X((enc)->bpe, (link)->inner.bpe)                                                               \
    X((enc)->entries, (link)->inner.entries)                                                       \
    X((enc)->n2, (link)->inner.n2)

typedef struct __attribute__((packed)) {
    uint64_t size;
    uint32_t nfilters;
    uint32_t options;
    uint32_t growth;
    dumpedChainLink links[];
} dumpedChainHeader;

static SBLink *getLinkPos(const SBChain *sb, long long curIter, size_t *offset) {
    // printf("Requested %lld\n", curIter);

    curIter--;
    SBLink *link = NULL;

    // Read iterator
    size_t seekPos = 0;

    for (size_t ii = 0; ii < sb->nfilters; ++ii) {
        if (seekPos + sb->filters[ii].inner.bytes > curIter) {
            link = sb->filters + ii;
            break;
        } else {
            seekPos += sb->filters[ii].inner.bytes;
        }
    }
    if (!link) {
        return NULL;
    }

    curIter -= seekPos;
    *offset = curIter;
    return link;
}

const char *SBChain_GetEncodedChunk(const SBChain *sb, long long *curIter, size_t *len,
                                    size_t maxChunkSize) {
    // See into the offset.
    size_t offset = 0;
    SBLink *link = getLinkPos(sb, *curIter, &offset);

    if (!link) {
        *curIter = 0;
        return NULL;
    }

    *len = maxChunkSize;
    size_t linkRemaining = link->inner.bytes - offset;
    if (linkRemaining < *len) {
        *len = linkRemaining;
    }

    *curIter += *len;
    // printf("Returning offset=%lu\n", offset);
    return (const char *)(link->inner.bf + offset);
}

char *SBChain_GetEncodedHeader(const SBChain *sb, size_t *hdrlen) {
    *hdrlen = sizeof(dumpedChainHeader) + (sizeof(dumpedChainLink) * sb->nfilters);
    dumpedChainHeader *hdr = RedisModule_Calloc(1, *hdrlen);
    hdr->size = sb->size;
    hdr->nfilters = sb->nfilters;
    hdr->options = sb->options;
    hdr->growth = sb->growth;

    for (size_t ii = 0; ii < sb->nfilters; ++ii) {
        dumpedChainLink *dstlink = &hdr->links[ii];
        SBLink *srclink = sb->filters + ii;

#define X(encfld, srcfld) encfld = srcfld;
        X_ENCODED_LINK(X, dstlink, srclink)
#undef X
    }
    return (char *)hdr;
}

void SB_FreeEncodedHeader(char *s) { RedisModule_Free(s); }

SBChain *SB_NewChainFromHeader(const char *buf, size_t bufLen, const char **errmsg) {
    const dumpedChainHeader *header = (const void *)buf;
    if (bufLen < sizeof(dumpedChainHeader)) {
        *errmsg = "ERR received bad data"; // LCOV_EXCL_LINE
        return NULL;                       // LCOV_EXCL_LINE
    }

    if (bufLen != sizeof(*header) + (sizeof(header->links[0]) * header->nfilters)) {
        *errmsg = "ERR received bad data"; // LCOV_EXCL_LINE
        return NULL;                       // LCOV_EXCL_LINE
    }

    SBChain *sb = RedisModule_Calloc(1, sizeof(*sb));
    sb->filters = RedisModule_Calloc(header->nfilters, sizeof(*sb->filters));
    sb->nfilters = header->nfilters;
    sb->options = header->options;
    sb->size = header->size;
    sb->growth = header->growth;

    for (size_t ii = 0; ii < header->nfilters; ++ii) {
        SBLink *dstlink = sb->filters + ii;
        const dumpedChainLink *srclink = header->links + ii;
#define X(encfld, dstfld) dstfld = encfld;
        X_ENCODED_LINK(X, srclink, dstlink)
#undef X
        dstlink->inner.bf = RedisModule_Alloc(dstlink->inner.bytes);
        if (sb->options & BLOOM_OPT_FORCE64) {
            dstlink->inner.force64 = 1;
        }
    }

    return sb;
}

int SBChain_LoadEncodedChunk(SBChain *sb, long long iter, const char *buf, size_t bufLen,
                             const char **errmsg) {
    // Load the chunk
    size_t offset;
    iter -= bufLen;

    SBLink *link = getLinkPos(sb, iter, &offset);
    if (!link) {
        *errmsg = "ERR invalid offset - no link found"; // LCOV_EXCL_LINE
        return -1;                                      // LCOV_EXCL_LINE
    }

    if (bufLen > link->inner.bytes - offset) {
        *errmsg = "ERR invalid chunk - Too big for current filter"; // LCOV_EXCL_LINE
        return -1;                                                  // LCOV_EXCL_LINE
    }

    // printf("Copying to %p. Offset=%lu, Len=%lu\n", link, offset, bufLen);
    memcpy(link->inner.bf + offset, buf, bufLen);
    return 0;
}
