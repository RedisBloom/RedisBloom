#include "sb.h"
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

static int SBChain_AddLink(SBChain *chain, size_t size, double error_rate) {
    if (!chain->filters) {
        chain->filters = RedisModule_Calloc(1, sizeof(*chain->filters));
    } else {
        chain->filters =
            RedisModule_Realloc(chain->filters, sizeof(*chain->filters) * (chain->nfilters + 1));
    }

    SBLink *newlink = chain->filters + chain->nfilters;
    newlink->size = 0;
    chain->nfilters++;
    return bloom_init(&newlink->inner, size, error_rate);
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

int SBChain_Add(SBChain *sb, const void *data, size_t len) {
    // Does it already exist?
    bloom_hashval h = bloom_calc_hash(data, len);
    for (int ii = sb->nfilters - 1; ii >= 0; --ii) {
        if (bloom_check_h(&sb->filters[ii].inner, h)) {
            return 0;
        }
    }

    // Determine if we need to add more items?
    SBLink *cur = CUR_FILTER(sb);
    if (cur->size >= cur->inner.entries) {
        double error = cur->inner.error * pow(ERROR_TIGHTENING_RATIO, sb->nfilters + 1);
        if (SBChain_AddLink(sb, cur->inner.entries * 2, error) != 0) {
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
    bloom_hashval hv = bloom_calc_hash(data, len);
    for (int ii = sb->nfilters - 1; ii >= 0; --ii) {
        if (bloom_check_h(&sb->filters[ii].inner, hv)) {
            return 1;
        }
    }
    return 0;
}

SBChain *SB_NewChain(size_t initsize, double error_rate) {
    if (initsize == 0 || error_rate == 0) {
        return NULL;
    }
    SBChain *sb = RedisModule_Calloc(1, sizeof(*sb));
    if (SBChain_AddLink(sb, initsize, error_rate) != 0) {
        SBChain_Free(sb);
        sb = NULL;
    }
    return sb;
}
