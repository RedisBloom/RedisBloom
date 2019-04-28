#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "xxhash.h"
#include "cms.h"

#define CMS_SIGNATURE "COUNTMINSKETCH:1.0:"
#define CMS_SEED 0
#define MAGIC 2147483647  // 2^31-1 according to the paper
#define min(a, b) ((a) < (b) ? (a) : (b))

typedef struct CMSketch {
  long long counter;
  int depth;
  int width;
  int *vector;
  unsigned int *hashA, *hashB;
} CMSketch;

///////////////////////// INIT /////////////////////////
/* Creates a new sketch based with given dimensions. */
static CMSketch *NewCMSketch(RedisModuleKey *key, int width, int depth) {
    size_t slen = strlen(CMS_SIGNATURE) + sizeof(CMSketch) +
                    sizeof(int) * width * depth +     // vector
                    sizeof(unsigned int) * 2 * depth; // hashes

    if (RedisModule_StringTruncate(key, slen) != REDISMODULE_OK) {
        return NULL;
    }
    size_t dlen;
    char *dma =
        RedisModule_StringDMA(key, &dlen, REDISMODULE_READ | REDISMODULE_WRITE);

    size_t off = strlen(CMS_SIGNATURE);
    memcpy(dma, CMS_SIGNATURE, off);

    CMSketch *cms = (CMSketch *)&dma[off];
    cms->counter = 0;
    cms->width = width;
    cms->depth = depth;
    off += sizeof(CMSketch);
    cms->vector = (int *)&dma[off];
    off += sizeof(int) * width * depth;
    cms->hashA = (unsigned int *)&dma[off];
    off += sizeof(unsigned int) * depth;
    cms->hashB = (unsigned int *)&dma[off];

    srand(CMS_SEED);
    for (int i = 0; i < cms->depth; ++i) {
        cms->hashA[i] = rand() & MAGIC;
        cms->hashB[i] = rand() & MAGIC;
    }

    return cms;
}

int CMSInit_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);

    RedisModuleKey *key =
        RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);

    /* Verify that the key is empty. */
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithError(ctx, "ERR key already exists");
        return REDISMODULE_ERR;
    }

    /* Get width and depth. */
    long long width, depth;
    size_t cmdlen;
    const char *cmd = RedisModule_StringPtrLen(argv[0], &cmdlen);
    if (!strcasecmp("CMS.INITBYDIM", cmd)) {
        if ((RedisModule_StringToLongLong(argv[2], &width) != REDISMODULE_OK) ||
            (width < 1) || (width > UINT16_MAX)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid width");
        return REDISMODULE_ERR;
        }
        if ((RedisModule_StringToLongLong(argv[3], &depth) != REDISMODULE_OK) ||
            (depth < 1) || (depth > UINT16_MAX)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid depth");
        return REDISMODULE_ERR;
        }
    } else {
        double error, prob;
        if ((RedisModule_StringToDouble(argv[2], &error) != REDISMODULE_OK) ||
            (error < 0) || (error >= 1)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid error");
        return REDISMODULE_ERR;
        }
        if ((RedisModule_StringToDouble(argv[3], &prob) != REDISMODULE_OK) ||
            (prob < 0) || (prob >= 1)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid probabilty");
        return REDISMODULE_ERR;
        }
        width = ceil(2 / error);
        depth = ceil(log10f(prob) / log10f(0.5));
    }

    CMSketch *cms = NewCMSketch(key, width, depth);
    if(NULL == cms) {
        RedisModule_ReplyWithError(ctx,
                            "ERR could not truncate key to required size");
        return REDISMODULE_ERR;
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

///////////////////////// GET (static) /////////////////////////
static CMSketch *GetCMSketch(RedisModuleCtx *ctx, RedisModuleKey *key) {
    size_t dlen;
    char *dma = RedisModule_StringDMA(key, &dlen, REDISMODULE_READ);

    size_t off = strlen(CMS_SIGNATURE);
    if (strncmp(CMS_SIGNATURE, dma, off)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid signature");
        return NULL;
    }

    CMSketch *cms = (CMSketch *)&dma[off];
    off += sizeof(CMSketch);
    cms->vector = (int *)&dma[off];
    off += sizeof(int) * cms->width * cms->depth;
    cms->hashA = (unsigned int *)&dma[off];
    off += sizeof(unsigned int) * cms->depth;
    cms->hashB = (unsigned int *)&dma[off];

    return cms;
}

///////////////////////// INCREASE /////////////////////////
static void CMSIncrBy(CMSketch *cms, RedisModuleString **argv, int argc) {
    /* Loop over the input items and update their counts. */
    for (int i = 3; i < argc; i += 2) {
        size_t len;
        const char *item = RedisModule_StringPtrLen(argv[i - 1], &len); // why i - 1?
        long long value;
        RedisModule_StringToLongLong(argv[i], &value);
        cms->counter += value;
        for (int j = 0; j < cms->depth; ++j) {
        long long h = (cms->hashA[j] * XXH32(item, len, MAGIC) + cms->hashB[j]) & MAGIC;
        // TODO: check for over/underflow
        cms->vector[j * cms->width + h % cms->width] += value;
        }
    }
}

int CMSIncrBy_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if ((argc < 4) || (argc % 2 != 0)) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);

    /* Validate that all values are integers. */
    for (int i = 3; i < argc; i += 2) {
        long long l;
        // TODO: check for integer over/underflow
        if ((RedisModule_StringToLongLong(argv[i], &l) != REDISMODULE_OK)) {
        RedisModule_ReplyWithError(ctx, "ERR value is not a valid integer");
        return REDISMODULE_ERR;
        }
    }

    RedisModuleKey *key =
        RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);

    /* If the key is empty, initialize with the defaults. */
    int init = 0;
    CMSketch *cms;
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        init = 1;
    } else if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_STRING) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    if (init) {
        cms = NewCMSketch(key, 2000, 10);  // %0.01 error at %0.01 probabilty
    } else {
        cms = GetCMSketch(ctx, key);
    }
    if (!cms) {
        return REDISMODULE_ERR;
    }

    /* TODO: pass argv/c w/o parameters used. Only items */ 
    CMSIncrBy(cms, argv/* + 2*/, argc/* -2*/);

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

///////////////////////// Query /////////////////////////
static int CMSQuery(CMSketch *cms, RedisModuleString *str) {
    size_t len;
    const char *value = RedisModule_StringPtrLen(str, &len);
    long long h = (cms->hashA[0] * XXH32(value, len, MAGIC) + cms->hashB[0]) & MAGIC;
    int freq = cms->vector[h % cms->width];
    for (int j = 1; j < cms->depth; ++j) {
        h = (cms->hashA[j] * XXH32(value, len, MAGIC) + cms->hashB[j]) & MAGIC;
        freq = min(freq, cms->vector[j * cms->width + h % cms->width]);
    }
    return freq;
}

int CMSQuery_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);

    /* If the key is empty, return NULL, otherwise verify that it is a string. */
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    } else if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_STRING) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    CMSketch *cms = GetCMSketch(ctx, key);
    if (!cms) {
        return REDISMODULE_ERR;
    }

    /* Loop over the items and estimate their counts. */
    RedisModule_ReplyWithArray(ctx, argc - 2);
    for (int i = 2; i < argc; ++i) {
        int freq = CMSQuery(cms, argv[i]);

        RedisModule_ReplyWithLongLong(ctx, freq);
    }

    return REDISMODULE_OK;
}

///////////////////////// Query /////////////////////////
void CMSMerge(CMSketch *destsketch, 
              RedisModuleString **argv,
              long long numkeys, 
              RedisModuleKey **keys, 
              CMSketch **sketches, 
              long long *weights) {
    size_t destkey_strlen;
    const char *destkey_str = RedisModule_StringPtrLen(argv[1], &destkey_strlen);
    long long destweight = 0;
    size_t num_destkey_in_keys = 0;

    for (int i = 0; i < numkeys; i++) {
        size_t key_strlen;
        const char *key_str = RedisModule_StringPtrLen(argv[i + 3], &key_strlen);

        if (!strcasecmp(destkey_str, key_str)) {
            destweight += weights[i];

            RedisModuleKey *tmp_key = keys[i];
            keys[i] = keys[num_destkey_in_keys];
            keys[num_destkey_in_keys] = tmp_key;

            CMSketch *tmp_sketch = sketches[i];
            sketches[i] = sketches[num_destkey_in_keys];
            sketches[num_destkey_in_keys] = tmp_sketch;

            long long tmp_weight = weights[i];
            weights[i] = weights[num_destkey_in_keys];
            weights[num_destkey_in_keys] = tmp_weight;

            num_destkey_in_keys++;
        }
    }

    for (int i = 0; i < destsketch->depth; i++) {
        for (int j = 0; j < destsketch->width; j++) {
            destsketch->vector[i * destsketch->width + j] *= destweight;
        }
    }

    for (int i = num_destkey_in_keys; i < numkeys; i++) {
        for (int j = 0; j < destsketch->depth; j++) {
            for (int k = 0; k < destsketch->width; k++) {
                destsketch->vector[j * sketches[i]->width + k] +=
                weights[i] * sketches[i]->vector[j * sketches[i]->width + k];
            }
        }
    }
}
    /* CMS.MERGE destkey numkeys key [key ...] [WEIGHTS weight [weight ...]]
    */
int CMSMerge_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);

    long long numkeys;
    if ((RedisModule_StringToLongLong(argv[2], &numkeys) != REDISMODULE_OK) ||
        (numkeys < 1)) {
        RedisModule_ReplyWithError(ctx, "ERR invalid numkeys");
        return REDISMODULE_ERR;
    }

    if (RedisModule_IsKeysPositionRequest(ctx)) {
        RedisModule_KeyAtPos(ctx, 1);
        for (int i = 0; i < numkeys; i++) {
            RedisModule_KeyAtPos(ctx, i + 3);
        }
        return REDISMODULE_OK;
    }

    int use_weights;
    if (argc == 2 * numkeys + 4) {
        size_t strlen;
        const char *str = RedisModule_StringPtrLen(argv[numkeys + 3], &strlen);
        if (strcasecmp("weights", str) != 0) {
            RedisModule_ReplyWithError(ctx, "ERR syntax error");
            return REDISMODULE_ERR;
        }
        use_weights = 1;
    } else if (argc == numkeys + 3) {
        use_weights = 0;
    } else {
        return RedisModule_WrongArity(ctx);
    }

    /* Validate that all values are sketches. */
    RedisModuleKey **keys = RedisModule_PoolAlloc(ctx, numkeys * sizeof(RedisModuleKey *));
    CMSketch **sketches = RedisModule_PoolAlloc(ctx, numkeys * sizeof(CMSketch *));
    long long *weights = RedisModule_PoolAlloc(ctx, numkeys * sizeof(long long));

    for (int i = 0; i < numkeys; i++) {
        keys[i] = RedisModule_OpenKey(ctx, argv[i + 3], REDISMODULE_READ);

        if (RedisModule_KeyType(keys[i]) != REDISMODULE_KEYTYPE_STRING) {
            RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
            return REDISMODULE_ERR;
        }

        sketches[i] = GetCMSketch(ctx, keys[i]);
        if (!sketches[i]) {
            RedisModule_ReplyWithError(ctx, "ERR cannot open a key");
            return REDISMODULE_ERR;
        }

        if ((sketches[0]->depth != sketches[i]->depth) ||
            (sketches[0]->width != sketches[i]->width)) {
            RedisModule_ReplyWithError(ctx, "ERR incompatible sketch");
            return REDISMODULE_ERR;
        }

        if (use_weights) {
            if (RedisModule_StringToLongLong(argv[i + numkeys + 4], &weights[i]) != REDISMODULE_OK) {
                RedisModule_ReplyWithError(ctx, "ERR invalid weight");
                return REDISMODULE_ERR;
            }
        } else {
        weights[i] = 1;
        }
    }

    RedisModuleKey *destkey =
        RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    CMSketch *destsketch;

    if (RedisModule_KeyType(destkey) == REDISMODULE_KEYTYPE_EMPTY) {
        destsketch = NewCMSketch(destkey, sketches[0]->width, sketches[0]->depth);
    } else if (RedisModule_KeyType(destkey) == REDISMODULE_KEYTYPE_STRING) {
        destsketch = GetCMSketch(ctx, destkey);

        if ((destsketch->depth != sketches[0]->depth) ||
            (destsketch->width != sketches[0]->width)) {
            RedisModule_ReplyWithError(ctx, "ERR incompatible sketch");
            return REDISMODULE_ERR;
        }
    } else {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    CMSMerge(destsketch, argv, numkeys, keys, sketches, weights);

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

/* CMS.DEBUG key
*/
int CMSDebug_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);

    /* Verify that the key is empty or a string. */
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_STRING) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    CMSketch *s = GetCMSketch(ctx, key);
    if (!s) {
        return REDISMODULE_ERR;
    }

    RedisModule_ReplyWithArray(ctx, 4);
    RedisModule_ReplyWithString(
        ctx, RedisModule_CreateStringPrintf(ctx, "Count: %lld", s->counter));
    RedisModule_ReplyWithString(
        ctx, RedisModule_CreateStringPrintf(ctx, "Width: %d", s->width));
    RedisModule_ReplyWithString(
        ctx, RedisModule_CreateStringPrintf(ctx, "Depth: %d", s->depth));
    RedisModule_ReplyWithString(
        ctx, RedisModule_CreateStringPrintf(ctx, "Size: %d",
                                            RedisModule_ValueLength(key)));
    return REDISMODULE_OK;
}