#include <stdint.h>
#include "murmurhash2.h"

typedef struct {
  uint64_t a;
  uint64_t b;
} TwoHash_t;


static inline void getTwoHash(TwoHash_t *hash, const void *key, int len) {
  hash->a = MurmurHash64A_Bloom(key, len, 0);
  hash->b = MurmurHash64A_Bloom(key, len, 1);
}

static inline uint64_t getHash(TwoHash_t hash, int seed) {
  return hash.a + seed * hash.b;
}

