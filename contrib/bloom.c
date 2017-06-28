/*
 *  Copyright (c) 2012-2017, Jyri J. Virkki
 *  All rights reserved.
 *
 *  This file is under BSD license. See LICENSE file.
 */

/*
 * Refer to bloom.h for documentation on the public interfaces.
 */

#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "bloom.h"
#include "murmurhash2.h"

#define MAKESTRING(n) STRING(n)
#define STRING(n) #n

#ifndef BLOOM_CALLOC
#define BLOOM_CALLOC calloc
#define BLOOM_FREE free
#endif

#define MODE_READ 0
#define MODE_WRITE 1

inline static int test_bit_set_bit(unsigned char *buf, unsigned int x, int mode) {
    unsigned int byte = x >> 3;
    unsigned char c = buf[byte]; // expensive memory access
    unsigned int mask = 1 << (x % 8);

    if (c & mask) {
        return 1;
    } else {
        if (mode == MODE_WRITE) {
            buf[byte] = c | mask;
        }
        return 0;
    }
}

bloom_hashval bloom_calc_hash(const void *buffer, int len) {
    bloom_hashval rv;
    rv.a = murmurhash2(buffer, len, 0x9747b28c);
    rv.b = murmurhash2(buffer, len, rv.a);
    return rv;
}

static int bloom_check_add(struct bloom *bloom, bloom_hashval hashval, int mode) {
    register unsigned int x;
    register unsigned int i;
    int found_unset = 0;

    for (i = 0; i < bloom->hashes; i++) {
        x = (hashval.a + i * hashval.b) % bloom->bits;
        if (!test_bit_set_bit(bloom->bf, x, mode)) {
            // Was not set
            if (mode == MODE_READ) {
                return 0;
            }
            found_unset = 1;
        }
    }
    if (mode == MODE_READ) {
        return 1;
    }
    return found_unset;
}

int bloom_init_size(struct bloom *bloom, int entries, double error, unsigned int cache_size) {
    return bloom_init(bloom, entries, error);
}

int bloom_init(struct bloom *bloom, int entries, double error) {
    if (entries < 1 || error == 0) {
        return 1;
    }

    bloom->entries = entries;
    bloom->error = error;

    double num = log(bloom->error);
    double denom = 0.480453013918201; // ln(2)^2
    bloom->bpe = -(num / denom);

    double dentries = (double)entries;
    bloom->bits = (int)(dentries * bloom->bpe);

    if (bloom->bits % 8) {
        bloom->bytes = (bloom->bits / 8) + 1;
    } else {
        bloom->bytes = bloom->bits / 8;
    }

    bloom->hashes = (int)ceil(0.693147180559945 * bloom->bpe); // ln(2)

    bloom->bf = (unsigned char *)BLOOM_CALLOC(bloom->bytes, sizeof(unsigned char));
    if (bloom->bf == NULL) {
        return 1;
    }

    return 0;
}

int bloom_check_h(const struct bloom *bloom, bloom_hashval hash) {
    return bloom_check_add((void *)bloom, hash, MODE_READ);
}

int bloom_check(const struct bloom *bloom, const void *buffer, int len) {
    return bloom_check_h(bloom, bloom_calc_hash(buffer, len));
}

int bloom_add_h(struct bloom *bloom, bloom_hashval hash) {
    return !bloom_check_add(bloom, hash, MODE_WRITE);
}

int bloom_add(struct bloom *bloom, const void *buffer, int len) {
    return bloom_add_h(bloom, bloom_calc_hash(buffer, len));
}

void bloom_print(struct bloom *bloom) {
    printf("bloom at %p\n", (void *)bloom);
    printf(" ->entries = %d\n", bloom->entries);
    printf(" ->error = %f\n", bloom->error);
    printf(" ->bits = %d\n", bloom->bits);
    printf(" ->bits per elem = %f\n", bloom->bpe);
    printf(" ->bytes = %d\n", bloom->bytes);
    printf(" ->hash functions = %d\n", bloom->hashes);
}

void bloom_free(struct bloom *bloom) { BLOOM_FREE(bloom->bf); }

const char *bloom_version() { return MAKESTRING(BLOOM_VERSION); }
