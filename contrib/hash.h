#include <stdint.h>

typedef struct {
  uint64_t a;
  uint64_t b;
} TwoHash_t;


uint64_t MurmurHash64A(const void *key, int len, unsigned int seed);

inline void getTwoHash(TwoHash_t *hash, const void *key, int len) {
  hash->a = MurmurHash64A(key, len, 0);
  hash->b = MurmurHash64A(key, len, 1);
}

inline uint64_t getHash(TwoHash_t hash, int seed) {
  return hash.a + seed * hash.b;
}

