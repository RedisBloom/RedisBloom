/* Compile using
 * gcc test_topk.c  -I../  -I../src ../contrib/MurmurHash2.c -lm
 */
 
#include <stdio.h> // printf
#include <stdlib.h>
#include <string.h> // strlen
#include <math.h>   // rand
#include <time.h>   // clock

#include "topk.h"
#include "topk.c"

#define TEST_SIZE 300
#define PRINT(str) printf("%s\n", str)

static void printHeap(TopK *topk) {
    for(int i = 0; i < topk->k; ++i) {
        if(ceil(log2(i + 1)) == floor(log2(i + 1))) printf("\n");
        if(topk->heap[i].item != NULL) printf("%s %u\t\t", topk->heap[i].item, topk->heap[i].count);
    }
    printf("\n\n");
}

int heapTest() {
    HeapBucket array[7] = { 0 };
    array[0].count = 5;
    heapifyDown(array, 7, 0);
    array[0].count = 51;
    heapifyDown(array, 7, 0);
    array[0].count = 12;
    heapifyDown(array, 7, 0);
    array[0].count = 2;
    heapifyDown(array, 7, 0);
    array[0].count = 21;
    heapifyDown(array, 7, 0);

    array[0].count = 18;
    heapifyDown(array, 7, 0);
    array[0].count = 8;
    heapifyDown(array, 7, 0);
    array[0].count = 14;
    heapifyDown(array, 7, 0);
    array[0].count = 35;
    heapifyDown(array, 7, 0);
    array[0].count = 19;
    heapifyDown(array, 7, 0);

    return 0;
}

static void topkMultiAdd(TopK *topk, const char *str, size_t strlen, int repeat) {

}

int controlledTest() {
    TopK *topk = TopK_Create(3, 100, 3, 0.925);
    TopK_Add(topk, "1", 2, 1);
    TopK_Add(topk, "2", 2, 1);
    TopK_Add(topk, "3", 2, 1);
    TopK_Add(topk, "4", 2, 1);
    TopK_Add(topk, "1", 2, 1);
    TopK_Add(topk, "1", 2, 1);
    TopK_Add(topk, "4", 2, 1);
    TopK_Add(topk, "3", 2, 1);
    TopK_Add(topk, "4", 2, 1);
    TopK_Add(topk, "1", 2, 1);
    TopK_Add(topk, "3", 2, 1);
    TopK_Add(topk, "4", 2, 1);
    TopK_Add(topk, "1", 2, 1);
    TopK_Add(topk, "1", 2, 1);
    TopK_Add(topk, "2", 2, 1);
    TopK_Add(topk, "2", 2, 1);
    TopK_Add(topk, "2", 2, 1);
    
    printHeap(topk);
    TopK_Destroy(topk);
    
    return 0;
}

int testLong() {
    int arr[TEST_SIZE][2] = { 0 }, total = 0, idx = 0;
    char str[TEST_SIZE * 10] = { 0 };
    TopK *topk = TopK_Create(31, TEST_SIZE/6, 5, 0.925);
  //  char str[12] = { 0 };
    char *runner = str;

    srand(10);
    for(int i = 0; i < TEST_SIZE; ++i) {
        arr[i][0] = arr[i][1] = pow(rand() % TEST_SIZE, 2);
        total += arr[i][0];
        sprintf(runner, "%d", i);
        runner +=10;
    }

    for(int i = 0; i < total - pow(TEST_SIZE, 2) * 3;) {
        idx = rand() % TEST_SIZE;
        if(arr[idx][0] > 0) {
            TopK_Add(topk, str + 10 * idx, strlen(str + 10 * idx), 1);
            --arr[idx][0];
            ++i;
        }
    }

    printHeap(topk);

    for(int i = 0; i < TEST_SIZE; ++i) {
        if(arr[i][1] > TEST_SIZE * TEST_SIZE * 3 / 5) {
            printf("%d-%u\t", i, arr[i][1] - arr[i][0]);
        }
    }
    TopK_Destroy(topk);
    return 0;
}

int testDecay() {
    TopK *topk = TopK_Create(63, TEST_SIZE / 4, 5, 0.925);
    char str[32] = { 0 };

    for(int i = 0; i < 30; ++i) {
        for(int j = 0; j < TEST_SIZE; j += 20) {
            sprintf(str, "%d", j);
            TopK_Add(topk, str, strlen(str), 1);
        }
    }
    for(int i = 0; i < 10; ++i) {
        for(int j = 0; j < TEST_SIZE; j += 10) {
            sprintf(str, "%d", j);
            TopK_Add(topk, str, strlen(str), 1);
        }
    }
    printHeap(topk);
    for(int i = 0; i < 100; ++i) {
        for(int j = 0; j < TEST_SIZE; ++j) {
            sprintf(str, "%d", j);
            uint32_t len = strlen(str);
            TopK_Add(topk, str, len, 1);
        }
    }
    printHeap(topk);
    TopK_Destroy(topk);
    return 0;
}

int testIncrby() {
    TopK *topk = TopK_Create(3, 5, 3, 0.9);
    TopK_Add(topk, "1", 2, 1);
    TopK_Add(topk, "2", 2, 1);
    TopK_Add(topk, "3", 2, 1);
    printHeap(topk);
    TopK_Add(topk, "4", 2, 3);
    TopK_Add(topk, "5", 2, 1);
    printHeap(topk);
    TopK_Add(topk, "5", 2, 10);
    TopK_Add(topk, "1", 2, 5);
    TopK_Add(topk, "2", 2, 20);
    TopK_Add(topk, "3", 2, 30);
    TopK_Add(topk, "1", 2, 5);
    printHeap(topk);
    TopK_Add(topk, "5", 2, 10);
    TopK_Add(topk, "4", 2, 5);
    TopK_Add(topk, "4", 2, 15);
    printHeap(topk);

    return 0;
}


int main() {
//    heapTest();
//    controlledTest();
//    testLong();
//    testDecay();
    testIncrby();

    return 0;
}