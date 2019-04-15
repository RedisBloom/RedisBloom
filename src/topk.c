/*
* topk - an almost deterministic top k elements counter Redis Module
* Copyright (C) 2016 Redis Labs
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Includes 'ustime' from redis/src/server.c
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <math.h>
#include <sys/time.h>
#include <strings.h>

#include "topk.h"
#include "../contrib/vector.h"

#define RLMODULE_NAME "TOPK"
#define RLMODULE_VERSION "1.0.1"
#define RLMODULE_PROTO "1.0"
#define RLMODULE_DESC "Deterministic top k with percentiles"

#define REDIS_LOG(str) fprintf(stderr, "topk.so: %s\n", str);

#define TOPK_META_PREFIX "TOPK:1.0.1:1.0:"
#define TOPK_META_SCORE -1.0

#define TOPK_K_ERR "ERR k must be an integer greater than zero"
#define TOPK_ELEM_ERR "ERR elements can not start with the 'TOPK' prefix"
#define TOPK_MISSING_ERR "ERR missing TOPK meta element"
#define TOPK_INVALID_ERR "ERR invalid TOPK meta element"
#define TOPK_TOOMANY_ERR "ERR too many TOPK meta elements"
#define TOPK_PREC_ERR "ERR loosing focus - call TOPK.SHRINK first"
#define TOPK_ZCARD_ERR "ERR a valid TOPK zset must have ZCARD > 2"

struct topk_meta_s {
    long long offset;
    long long sum;
};

typedef struct topk_meta_s topk_meta;

struct zset_ele_s {
    RedisModuleString *ele;
    double score;
};

typedef struct zset_ele_s zset_ele;

/* Return the UNIX time in microseconds */
long long ustime(void) {
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long long)tv.tv_sec) * 1000000;
    ust += tv.tv_usec;
    return ust;
}

/* Creates a new meta struct. */
topk_meta *meta_new() {
    topk_meta *m;
    if ((m = malloc(sizeof(topk_meta))) == NULL) return NULL;

    m->offset = 0;
    m->sum = 0;

    return m;
}

/* Serializes the meta struct to a string buffer. */
char *meta_dump(const topk_meta *meta, size_t *len) {
    size_t prelen = strlen(TOPK_META_PREFIX);
    size_t size = prelen + sizeof(topk_meta);
    char *dump = calloc(size, sizeof(char));
    if (dump == NULL) return NULL;

    memcpy(dump, TOPK_META_PREFIX, prelen);
    memcpy(&dump[prelen], meta, sizeof(topk_meta));
    *len = size;
     return dump;
}

/* Loads a meta struct from its serialized form. */
topk_meta *meta_load(const char *ser, const size_t len) {
    size_t prelen = strlen(TOPK_META_PREFIX);

    /* Check buffer size. */
    if (len != prelen + sizeof(topk_meta)) return NULL;

    /* Check prefix. */
    if (strncmp(TOPK_META_PREFIX, ser, prelen)) return NULL;

    topk_meta *meta;
    if ((meta = malloc(sizeof(topk_meta))) == NULL) return NULL;

    memcpy(meta, &ser[prelen], sizeof(topk_meta));

    /* Sanity. */
    if ((meta->offset > 0) || (meta->sum < 1)) return NULL;

    return meta;
}

/* Helper: gets the meta from a zset and potentially removes it */
topk_meta *GetMeta(RedisModuleCtx *ctx, RedisModuleKey *key, int remove) {
    topk_meta *meta = NULL;

    size_t size = RedisModule_ValueLength(key);
    if (size < 2) { /* TOPK's ZCARD must be 2 or above */
        RedisModule_ReplyWithError(ctx, TOPK_ZCARD_ERR);
        return NULL;
    }

    size_t meta_count = 0;
    double score;
    RedisModuleString *ele;
    RedisModule_ZsetFirstInScoreRange(key, REDISMODULE_NEGATIVE_INFINITE,
                                        TOPK_META_SCORE, 0, 0);
    while (!RedisModule_ZsetRangeEndReached(key)) {
        if (meta_count) { /* There can be only one. */
            free(meta);
            RedisModule_ReplyWithError(ctx, TOPK_TOOMANY_ERR);
            return NULL;
        }

        ele = RedisModule_ZsetRangeCurrentElement(key, &score);

        if (score != TOPK_META_SCORE) { /* The one has to have the right score. */
            RedisModule_ReplyWithError(ctx, TOPK_INVALID_ERR);
            return NULL;
        }

        size_t len;
        const char *ser = RedisModule_StringPtrLen(ele, &len);
        if ((meta = meta_load(ser, len)) ==
            NULL) { /* The one has to load successfully. */
            RedisModule_ReplyWithError(ctx, TOPK_INVALID_ERR);
            return NULL;
        }

        RedisModule_ZsetRangeNext(key);
    }
    RedisModule_ZsetRangeStop(key);
    if (meta == NULL) {
        RedisModule_ReplyWithError(ctx, TOPK_MISSING_ERR);
        return NULL;
    }

    if (remove) RedisModule_ZsetRem(key, ele, NULL);

    return meta;
}

/* Helper: sets meta to zset and frees it. */
int SetMeta(RedisModuleCtx *ctx, RedisModuleKey *key, topk_meta *meta) {
    size_t len;
    char *dump = meta_dump(meta, &len);
    RedisModuleString *ele = RedisModule_CreateString(ctx, dump, len);
    RedisModule_ZsetAdd(key, TOPK_META_SCORE, ele, NULL);
    free(dump);
    free(meta);
    return 1;
}

/* Helper: sets Meta and frees vectors.  Must end with NULL pointer */
int RedisTopK_MallocError(RedisModuleCtx *ctx, RedisModuleKey *key, topk_meta *meta, ...)
{
    SetMeta(ctx, key, meta);
    va_list ap; 
    va_start(ap, meta);  
    void *ptr = NULL;
    while((ptr = va_arg(ap, void *)) != NULL) {
        Vector_Free(ptr);
    }
    return RedisModule_ReplyWithError(ctx, "ERR could not allocate memory");
}

/* TOPK.SHRINK key [k]
 * Complexity: O(N) where N is the number of members in the Sorted Set.
 * Resets the global offset after applying it to all members, trims to k if
 * specified and not zero.
 * Reply: String, "OK"
*/
int TopKShrink_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv,
                            int argc) {
    if ((argc != 2) && (argc != 3)) return RedisModule_WrongArity(ctx);
    RedisModule_AutoMemory(ctx);

    /* Open key and verify it is empty or a zset. */
    RedisModuleKey *key =
        RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY)
        return RedisModule_ReplyWithSimpleString(ctx, "OK");
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_ZSET)
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);

    /* Get meta and size */
    topk_meta *meta;
    if ((meta = GetMeta(ctx, key, 1)) == NULL) return REDISMODULE_ERR;
    size_t size = RedisModule_ValueLength(key);

    /* Get k, if specified, else assume at least the current size. */
    long long k = 0;
    if ((argc == 3) &&
        (RedisModule_StringToLongLong(argv[2], &k) != REDISMODULE_OK) && (k < 0))
        return RedisModule_ReplyWithError(ctx, TOPK_K_ERR);
    if (k == 0) k = size;

    /* Adjust offset with overflow, if any. */
    meta->offset -= (k < size ? 0 : size - k);

    /* Decrement all scores by offset. */
    int flags;
    /* TODO: once RedisModule_ZsetRemCurrentElement and/or ZsetIncrCurrentElement
    * are implemented, switch to it. */
    Vector *vupd = NewVector(void *, 1);
    Vector *vrem = NewVector(void *, 1);

    /* Add to lupd all elements for update, and to lrem those with final scores
    * leq to offset. */
    double score;
    RedisModuleString *ele;
    RedisModule_ZsetFirstInScoreRange(key, 0.0, REDISMODULE_POSITIVE_INFINITE, 1,
                                        0);
    while (!RedisModule_ZsetRangeEndReached(key)) {
        ele = RedisModule_ZsetRangeCurrentElement(key, &score);
        if (score + meta->offset < 1) {
            Vector_Push(vrem, ele);
            meta->sum -= score;
        } else
            Vector_Push(vupd, ele);
        RedisModule_ZsetRangeNext(key);
    }
    RedisModule_ZsetRangeStop(key);

    /* Update the sum with the to be updated elements' shift. */
    meta->sum += Vector_Size(vupd) * meta->offset;

    /* Now actually perform the removals. */
    int i;
    for (i = 0; i < Vector_Size(vrem); i++) {
        Vector_Get(vrem, i, &ele);
        RedisModule_ZsetRem(key, ele, NULL);
    }
    Vector_Free(vrem);

    /* Now actually perform the updates. */
    for (i = 0; i < Vector_Size(vupd); i++) {
        Vector_Get(vupd, i, &ele);
        flags = 0;
        RedisModule_ZsetIncrby(key, meta->offset, ele, &flags, &score);
    }
    Vector_Free(vupd);

    /* Reset the global offset. */
    meta->offset = 0;
    SetMeta(ctx, key, meta);

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

/* TOPK.ADD key k elem [elem ...]
 * Complexity: O(log(N)), where N is the size of the zset.
 * Adds elements to a TopK zset.
 * Notes:
 *   - elem can't begin with the _'TOPK'_ prefix.
 *   - If k is zero, the zset keeps growing.
 *   - Using different k is perfectly possible and could be even interesting
 *   - Optimization in the variadic form: first incr existing elements, then do
 *     one sweep of decr, then remove < 1 as needed. This may screw the
 *     probabilities (someone can  probably dis/prove),
 *     so just use non variadic calls to play safe if you want.
 * An error is returned when:
 *   - The operation requires running `TOPK.SHRINK` before.
 *   - The Sorted Set exists and doesn't sport the meta
 * Reply: Integer. If positive, it is the number of new elements added. If
 * negative, it is the number of elements removed due to k overflow. If 0, it
 * mean that only the offset/scores were updated.
*/
int TopKAdd_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv,
                         int argc) {
    if (argc < 4) return RedisModule_WrongArity(ctx);

    RedisModule_AutoMemory(ctx);

    /* Open key and verify it is empty or a zset. */
    RedisModuleKey *key =
        RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    if ((RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) &&
        (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_ZSET)) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    /* Get k. */
    long long k = 0;
    if ((RedisModule_StringToLongLong(argv[2], &k) != REDISMODULE_OK) || (k < 0))
        return RedisModule_ReplyWithError(ctx, TOPK_K_ERR);

    /* Check that no new element begins with TOPK_META_PREFIX */
    size_t lenpref = strlen(TOPK_META_PREFIX);
    size_t i;
    for (i = 3; i < argc; i++) {
        size_t len;
        const char *arg = RedisModule_StringPtrLen(argv[i], &len);
        if ((len >= lenpref) && (!strncasecmp(TOPK_META_PREFIX, arg, lenpref)))
            return RedisModule_ReplyWithError(ctx, TOPK_ELEM_ERR);
    }

    topk_meta *meta;
    double score;
    int flags = 0;

    /* Get or initialize meta. */
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY)
        meta = meta_new();
    else if ((meta = GetMeta(ctx, key, 1)) == NULL)
        return REDISMODULE_ERR;

    /* We don't want to get too close to double's precision, so only use up to 52
    * bits
    * or call for a shrink. This makes some assumptions about cases where
    * precision is lost:
    *  - in the global offset due to a k overflow
    *  - in topk elements with scores being updated
    * Also, we'll call for a shrink if the sum of scores reaches near the 64 bit
    * area.
    */
    long long maxp = (long long)1 << 51;
    if (meta->offset <= -maxp)
        return RedisModule_ReplyWithError(ctx, TOPK_PREC_ERR);

    size_t size = RedisModule_ValueLength(key);
    if (size > 0) {
        /* Get maximum score in zset. */
        RedisModule_ZsetLastInScoreRange(key, 1.0, REDISMODULE_POSITIVE_INFINITE, 0,
                                        0);
        RedisModule_ZsetRangeCurrentElement(key, &score);
        RedisModule_ZsetRangeStop(key);

        if ((score >= maxp) || (meta->sum >= (maxp << 11)))
            return RedisModule_ReplyWithError(ctx, TOPK_PREC_ERR);
    }

    /* Iterate and incr existing elements, only mind the new ones. */
    Vector *add = NewVector(void *, 1);
    if (add == NULL) RedisTopK_MallocError(ctx, key, meta, NULL);
    for (i = 3; i < argc; i++) {
        flags = REDISMODULE_ZADD_XX;
        RedisModule_ZsetIncrby(key, 1.0, argv[i], &flags, &score);
        if (flags == REDISMODULE_ZADD_NOP) {
            Vector_Push(add, argv[i]);
        // __vector_PushPtr(add,(void *)argv[i]);
        } else {
            meta->sum += 1;
        }
    }
    long long added = Vector_Size(add);

    /* Check if the zset exceeds k. */
    long long olen = RedisModule_ValueLength(key);
    long long ocap = k - olen;
    long long overflow = (k ? olen + added - k : 0);
    Vector *rem = NULL;
    long long remmed = 0;
    RedisModuleString *ele;
    zset_ele *zele;
    size_t j = overflow;

    /* An overflow means adjusting offset score and removing members beneath it.
    */
    if (overflow > 0) {
        meta->offset -= overflow;

        /* 1. No elements to remove
        * 2. Some (< overflow) elments to remove
        * 3. Overflow elements can be removed
        */
        /* Algorithm R. */
        rem = NewVector(zset_ele *, overflow);
        if (rem == NULL) RedisTopK_MallocError(ctx, key, meta, add, NULL);
        RedisModule_ZsetFirstInScoreRange(key, 0.0, -meta->offset, 1, 0);
        i = 0;
        while (!RedisModule_ZsetRangeEndReached(key) && j) {
            zele = malloc(sizeof(zset_ele));
            if (zele == NULL) RedisTopK_MallocError(ctx, key, meta, add, rem, NULL);
            zele->ele = RedisModule_ZsetRangeCurrentElement(key, &zele->score);
            Vector_Push(rem, zele);
            i++;
            j--;
            RedisModule_ZsetRangeNext(key);
        }
        remmed = Vector_Size(rem);

        srand(ustime());
        i = overflow + 1;
        while (!RedisModule_ZsetRangeEndReached(key) && remmed) {
            j = rand() % i;
            if (j < overflow) {
                Vector_Get(rem, j, &zele);
                zele->ele = RedisModule_ZsetRangeCurrentElement(key, &zele->score);
            }
            i++;
            RedisModule_ZsetRangeNext(key);
        }
        RedisModule_ZsetRangeStop(key);

        /* Now actually perform the removals. */
        for (i = 0; i < remmed; i++) {
            Vector_Get(rem, i, &zele);
            RedisModule_ZsetRem(key, zele->ele, NULL);
            meta->sum -= (long long)zele->score;
            free(zele);
        }
        Vector_Free(rem);

        /* In case there is less capacity than new elements, R downsample the new. */
        long long ncap = ocap + remmed;
        for (i = ncap; ((i < added) && ncap); i++) {
            j = rand() % i;
            if (j < remmed) {
                Vector_Get(add, j, &ele);
                Vector_Get(add, i, &ele);
                Vector_Put(add, j, ele);
            } else {
                Vector_Get(add, i, &ele);
            }
        }
        added = ncap;
    }

    /* Conclude by adding all new elements. */
    score = -(meta->offset) + 1;

    for (i = 0; i < added; i++) {
        flags = REDISMODULE_ZADD_NX;
        Vector_Get(add, i, &ele);
        RedisModule_ZsetAdd(key, score, ele, &flags);
        meta->sum += score;
    }
    Vector_Free(add);

    /* Store meta. */
    SetMeta(ctx, key, meta);

    if (overflow > 0)
        RedisModule_ReplyWithLongLong(ctx, -remmed);
    else
        RedisModule_ReplyWithLongLong(ctx, added);
    return REDISMODULE_OK;
}

/* TOPK.PRANGE key from to [DESC|ASC]
 * Returns the elements in the percentile range.
 * Both `from` and `to` must be between 0 and 100, and `from` must be less than
 * or equal to `to`.
 * The optional switch determines the reply's sort order, where `DESC` (the
 * default) means ordering
 * from highest to lowest frequency.
 * Reply: Array of strings.
 */
int TopKPRange_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv,
                            int argc) {
    if ((argc != 4) && (argc != 5)) return RedisModule_WrongArity(ctx);
    RedisModule_AutoMemory(ctx);

    /* Open key and verify it is empty or a zset. */
    RedisModuleKey *key =
        RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY)
        return RedisModule_ReplyWithNull(ctx);
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_ZSET)
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);

    /* Get from here to there. */
    long long from;
    if (((RedisModule_StringToLongLong(argv[2], &from)) != REDISMODULE_OK) ||
        (from < 0) || (from > 100)) {
        return RedisModule_ReplyWithError(
            ctx, "ERR from has to be an integer between 0 and 100");
    }
    long long to;
    if (((RedisModule_StringToLongLong(argv[3], &to)) != REDISMODULE_OK) ||
        (to < 0) || (to > 100)) {
        return RedisModule_ReplyWithError(
            ctx, "ERR to has to be an integer between 0 and 100");
    }
    if (from > to)
        return RedisModule_ReplyWithError(
            ctx, "ERR from has to be lower or equal to to");

    /* Determine the order. */
    int asc = 0;
    if (argc == 5) {
        size_t len;
        const char *str = RedisModule_StringPtrLen(argv[4], &len);
        if (!strcasecmp("asc", str))
            asc = 1;
        else if (!strcasecmp("desc", str))
            asc = 0;
        else
            return RedisModule_ReplyWithError(ctx, "ERR valid order is ASC or DESC");
    }

    /* Verify that this is a valid TOPK zset. */
    topk_meta *meta = GetMeta(ctx, key, 0);
    if (meta == NULL) return REDISMODULE_ERR;

    /* Make it so. */
    /* Sum the scores of elements until and excluding the 'from' percentile. */
    double score;
    RedisModuleString *ele;
    long long cl = 0;
    long long prank = 0;
    RedisModule_ZsetFirstInScoreRange(key, 0.0, REDISMODULE_POSITIVE_INFINITE,
                                      1, 0);
    while ((!RedisModule_ZsetRangeEndReached(key)) && (prank < from)) {
        ele = RedisModule_ZsetRangeCurrentElement(key, &score);
        cl += (long long)score;
        prank = (((double)cl + score) / (double)meta->sum) * 100;
        RedisModule_ZsetRangeNext(key);
    }

    Vector *reply = NewVector(void *, 1);
    if (reply == NULL) {
        free(meta);
        return RedisModule_ReplyWithError(ctx,
                "ERR could not allocate memory for reply");
    }

    while ((!RedisModule_ZsetRangeEndReached(key)) && (prank <= to)) {
        ele = RedisModule_ZsetRangeCurrentElement(key, &score);
        cl += (long long)score;
        prank = (((double)cl + score) / (double)meta->sum) * 100;
        Vector_Push(reply, ele);
        RedisModule_ZsetRangeNext(key);
    }
    RedisModule_ZsetRangeStop(key);

    /* Prepare the reply. */
    RedisModule_ReplyWithArray(ctx, Vector_Size(reply));
    int i;
    if (asc) {
        for (i = 0; i < Vector_Size(reply); i++) {
            Vector_Get(reply, i, &ele);
            RedisModule_ReplyWithString(ctx, ele);
        }
    } else {
        for (i = Vector_Size(reply) - 1; i >= 0; i--) {
            Vector_Get(reply, i, &ele);
            RedisModule_ReplyWithString(ctx, ele);
        }
    }

    Vector_Free(reply);
    free(meta);
    return REDISMODULE_OK;
}

/* TOPK.PRANK key ele [ele ...]
 * Returns the percentile rank for the element.
 * Reply: Array of integers, nil if key or element not found
*/
int TopKPRank_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv,
                           int argc) {
    if (argc < 2) return RedisModule_WrongArity(ctx);
    RedisModule_AutoMemory(ctx);

    /* Open key and verify it is empty or a zset. */
    RedisModuleKey *key =
        RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY)
        return RedisModule_ReplyWithNull(ctx);
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_ZSET)
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);

    /* Verify that this is a valid TOPK zset. */
    topk_meta *meta = GetMeta(ctx, key, 0);
    if (meta == NULL) return REDISMODULE_ERR;
    free(meta);

    RedisModule_ReplyWithArray(ctx, argc - 2);
    int i;
    for (i = 2; i < argc; i++) {
        /* https://en.wikipedia.org/wiki/Percentile_rank */
        double score;
        if (RedisModule_ZsetScore(key, argv[i], &score) != REDISMODULE_OK) {
            RedisModule_ReplyWithNull(ctx);
            continue;
        }

        /* TODO: if variadic, it makes sense to reduce the work by sorting args by
        * rank &
        * incrementally computing the sums */
        /* Sum the scores of elements just before the current. */
        double ts;
        long long cl = 0;
        RedisModule_ZsetFirstInScoreRange(key, 0.0, score, 1, 1);
        while (!RedisModule_ZsetRangeEndReached(key)) {
            RedisModule_ZsetRangeCurrentElement(key, &ts);
            cl += (long long)ts;
            RedisModule_ZsetRangeNext(key);
        }
        RedisModule_ZsetRangeStop(key);

        /* Sum the scores of the elements with the same score as current. */
        long long ce = 0;
        RedisModule_ZsetFirstInScoreRange(key, score, score, 0, 0);
        while (!RedisModule_ZsetRangeEndReached(key)) {
            RedisModule_ZsetRangeCurrentElement(key, &ts);
            ce += (long long)ts;
            RedisModule_ZsetRangeNext(key);
        }
        RedisModule_ZsetRangeStop(key);

        long long prank = (((double)cl + (double)ce) / (double)meta->sum) * 100;
        RedisModule_ReplyWithLongLong(ctx, prank);
    }
    return REDISMODULE_OK;
}

/* TOPK.DEBUG subcommand key [arg]
 * subcommands:
 *  - MAKE: fills 'key' with 1..'arg' (has to be integer) observations with
 * matching freqs
 *  - SHOW: shows meta information about 'key'
*/
int TopKDebug_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv,
                           int argc) {
    if ((argc < 3) || (argc > 4)) return RedisModule_WrongArity(ctx);

    RedisModule_AutoMemory(ctx);

    RedisModuleKey *key =
        RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ | REDISMODULE_WRITE);

    size_t len;
    const char *cmd = RedisModule_StringPtrLen(argv[1], &len);
    if (!strcasecmp("show", cmd)) {
        if (argc != 3) return RedisModule_WrongArity(ctx);
        if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY)
            return RedisModule_ReplyWithNull(ctx);
        if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_ZSET)
            return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);

        topk_meta *meta = GetMeta(ctx, key, 0);
        if (meta == NULL) return REDISMODULE_ERR;

        char buff[128];
        sprintf(buff, "zset size: %ld (+1 meta), score offset: %lld, sum: %lld",
                RedisModule_ValueLength(key) - 1, meta->offset, meta->sum);
        RedisModule_ReplyWithSimpleString(ctx, buff);
    } else if (!strcasecmp("make", cmd)) {
        if (argc != 4) return RedisModule_WrongArity(ctx);
        if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY)
            return RedisModule_ReplyWithError(ctx, "ERR target key exists");
        long long size;
        if ((RedisModule_StringToLongLong(argv[3], &size) != REDISMODULE_OK) ||
            (size < 1)) {
            return RedisModule_ReplyWithError(ctx,
                                            "ERR size must be greater than zero");
        }
        topk_meta *meta = meta_new();
        while (size) {
            RedisModuleString *ele = RedisModule_CreateStringFromLongLong(ctx, size);
            RedisModule_ZsetAdd(key, (double)size, ele, NULL);
            meta->sum += size;
            size--;
        }
        SetMeta(ctx, key, meta);
        RedisModule_ReplyWithSimpleString(ctx, "OK");
    } else {
        return RedisModule_ReplyWithError(ctx, "ERR unknown debug subcommand");
    }

    return REDISMODULE_OK;
}


/* Unit test entry point for the module. */
int TopKTest_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    RedisModule_ReplyWithSimpleString(ctx, "PASS");
    return REDISMODULE_OK;
}
