#include <stdlib.h> // malloc
#include <stdio.h>  // printf
#include <math.h>   // q, ceil
#include <assert.h> // assert

#include "cms.h"
#include "contrib/murmurhash2.h"

#define BIT64 64
#define CMS_HASH(item, itemlen, i) MurmurHash2(item, itemlen, i)

CMSketch *NewCMSketch(size_t width, size_t depth) {
    assert(width > 0);
    assert(depth > 0);

    CMSketch *cms = CMS_CALLOC(1, sizeof(CMSketch));

    cms->width = width;
    cms->depth = depth;
    cms->counter = 0;
    cms->array = CMS_CALLOC(width * depth, sizeof(uint32_t));

    return cms;
}

void CMS_DimFromProb(size_t n, double overEstProb, double errProb, size_t *width, size_t *depth) {
    assert(overEstProb > 0 && overEstProb < 1);
    assert(errProb > 0 && errProb < 1);

    /*  With large enough n, width can be reduced and maintain low
        over estimate probability */
    *width = n * exp(1) / ceil(n * pow(overEstProb, 2));
    *depth = -ceil(log(errProb));
}

void CMS_Destroy(CMSketch *cms) {
    assert(cms);

    CMS_FREE(cms->array);
    cms->array = NULL;

    CMS_FREE(cms);
}

void CMS_IncrBy(CMSketch *cms, const char *item, size_t itemlen, size_t value) {
    assert(cms);
    assert(item);

    for (size_t i = 0; i < cms->depth; ++i) {
        uint32_t hash = CMS_HASH(item, itemlen, i);
        cms->array[(hash % cms->width) + (i * cms->width)] += value;
    }
    cms->counter += value;
}

size_t CMS_Query(CMSketch *cms, const char *item, size_t itemlen) {
    assert(cms);
    assert(item >= 0);

    size_t temp = 0, res = (size_t)-1;

    for (size_t i = 0; i < cms->depth; ++i) {
        uint32_t hash = CMS_HASH(item, itemlen, i);
        temp = cms->array[(hash % cms->width) + (i * cms->width)];
        if (temp < res) {
            res = temp;
        }
    }

    return res;
}

void CMS_Merge(CMSketch *dest, size_t quantity, const CMSketch **src, const long long *weights) {
    assert(dest);
    assert(src);
    assert(weights);

    size_t itemCount = 0;
    size_t cmsCount = 0;
    size_t width = dest->width;
    size_t depth = dest->depth;

    for (size_t i = 0; i < depth; ++i) {
        for (size_t j = 0; j < width; ++j) {
            itemCount = 0;
            for (size_t k = 0; k < quantity; ++k) {
                itemCount += src[k]->array[(i * width) + j] * weights[k];
            }
            dest->array[(i * width) + j] = itemCount;
        }
    }

    for (size_t i = 0; i < quantity; ++i) {
        cmsCount += src[i]->counter * weights[i];
    }
    dest->counter = cmsCount;
}

void CMS_Print(const CMSketch *cms) {
    assert(cms);

    for (int i = 0; i < cms->depth; ++i) {
        for (int j = 0; j < cms->width; ++j) {
            printf("%d\t", cms->array[(i * cms->width) + j]);
        }
        printf("\n");
    }
    printf("\tCounter is %lu\n", cms->counter);
}

size_t CMS_GetCardinality(CMSketch *cms) {
    size_t width = cms->width, card = 0, count = 0;

    for (int i = 0; i < cms->depth; ++i, count = 0) {
        for (int j = 0; j < width; ++j) {
            count += !!cms->array[i * width + j];
        }
        if (count == cms->width)
            return cms->width;
        size_t tempCard = -(cms->width * log(1 - (count / (double)cms->width)));
        card = (card < tempCard) ? tempCard : card;
    }

    return card;
}