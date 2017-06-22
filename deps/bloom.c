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

static int bloom_check_add(struct bloom *bloom, const void *buffer, int len, int mode) {
    if (bloom->ready == 0) {
        printf("bloom at %p not initialized!\n", (void *)bloom);
        return -1;
    }

    int hits = 0;
    register unsigned int a = murmurhash2(buffer, len, 0x9747b28c);
    register unsigned int b = murmurhash2(buffer, len, a);
    register unsigned int x;
    register unsigned int i;

    for (i = 0; i < bloom->hashes; i++) {
        x = (a + i * b) % bloom->bits;
        if (test_bit_set_bit(bloom->bf, x, mode)) {
            hits++;
        } else if (mode == MODE_READ) {
            // Don't care about the presence of all the bits. Just our own.
            return 0;
        }
    }

    return hits;
}

int bloom_init_size(struct bloom *bloom, int entries, double error, unsigned int cache_size) {
    return bloom_init(bloom, entries, error);
}

int bloom_init(struct bloom *bloom, int entries, double error) {
    bloom->ready = 0;

    if (entries < 1000 || error == 0) {
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

    bloom->ready = 1;
    return 0;
}

int bloom_check(struct bloom *bloom, const void *buffer, int len) {
    int rv = bloom_check_add(bloom, buffer, len, MODE_READ);
    return rv <= 0 ? rv : 1;
}

int bloom_add(struct bloom *bloom, const void *buffer, int len) {
    int rv = bloom_add_retbits(bloom, buffer, len);
    if (rv == 0) {
        return 1; // No new bits added
    } else if (rv < 0) {
        return rv;
    } else {
        return 0;
    }
}

int bloom_add_retbits(struct bloom *bloom, const void *buffer, int len) {
    int rv = bloom_check_add(bloom, buffer, len, MODE_WRITE);
    if (rv < 0) {
        return -1;
    } else {
        return bloom->hashes - rv;
    }
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

void bloom_free(struct bloom *bloom) {
    if (bloom->ready) {
        BLOOM_FREE(bloom->bf);
    }
    bloom->ready = 0;
}

const char *bloom_version() { return MAKESTRING(BLOOM_VERSION); }
