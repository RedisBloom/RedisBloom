#include "murmurhash2.h"

void main() {
    uint64_t arr[2] = { 1, 2 };

    (void)MurmurHash64A_Bloom(arr, 6, 0);
    (void)MurmurHash64A_Bloom(arr, 7, 0);

    (void)MurmurHash64B(arr, 1, 0);
    (void)MurmurHash64B(arr, 2, 0);
    (void)MurmurHash64B(arr, 3, 0);
    (void)MurmurHash64B(arr, 4, 0);
    (void)MurmurHash64B(arr, sizeof(uint64_t) * 2, 0);
    return 0;
}