#include <stdio.h> // printf
#include <stdlib.h>
#include <string.h> // strlen
#include <math.h>   // rand
#include <time.h>   // clock

#include "topk.h"
#include "topk.c"

#define TEST_SIZE 1000
#define PRINT(str) printf("%s\n", str)

static void printHeap(TopK *topk) {
    for(int i = 0; i < topk->k; ++i) {
        if(ceil(log2(i + 1)) == floor(log2(i + 1))) printf("\n");
        if(topk->heap[i].item != NULL) printf("%s %ul\t\t", topk->heap[i].item, topk->heap[i].count);
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
    TopK *topk = TopK_Create(3, 100, 3);
    TopK_Add(topk, "1", 2);
    TopK_Add(topk, "2", 2);
    TopK_Add(topk, "3", 2);
    TopK_Add(topk, "4", 2);
    TopK_Add(topk, "1", 2);
    TopK_Add(topk, "1", 2);
    TopK_Add(topk, "4", 2);
    TopK_Add(topk, "3", 2);
    TopK_Add(topk, "4", 2);
    TopK_Add(topk, "1", 2);
    TopK_Add(topk, "3", 2);
    TopK_Add(topk, "4", 2);
    TopK_Add(topk, "1", 2);
    TopK_Add(topk, "1", 2);
    TopK_Add(topk, "2", 2);
    TopK_Add(topk, "2", 2);
    TopK_Add(topk, "2", 2);
    
    printHeap(topk);

}

int test1() {
    int arr[TEST_SIZE][2] = { 0 }, total = 0, idx = 0;
    char str[TEST_SIZE * 10] = { 0 };
    TopK *topk = TopK_Create(31, TEST_SIZE/2, 5);
  //  char str[12] = { 0 };
    char *runner = str;

    srand(0);
    for(int i = 0; i < TEST_SIZE; ++i) {
        arr[i][0] = arr[i][1] = rand() % TEST_SIZE;
        total += arr[i][0];
        sprintf(runner, "%d", i);
        runner +=10;
    }

//   if used, total in loop below must be extended
   arr[10][0] = arr[10][1] = 2500;
   arr[15][0] = arr[15][1] = 2500;
   arr[76][0] = arr[76][1] = 2500;
   arr[34][0] = arr[34][1] = 2500;
   arr[88][0] = arr[88][1] = 2500;
   arr[60][0] = arr[60][1] = 2500;
   arr[40][0] = arr[40][1] = 2500;

    for(int i = 0; i < total + 10000;) {
        idx = rand() % TEST_SIZE;
        if(arr[idx][0] > 0) {
            TopK_Add(topk, str + 10 * idx, strlen(str + 10 * idx));
            --arr[idx][0];
            ++i;
        }
    }

    printHeap(topk);

    for(int i = 0; i < TEST_SIZE; ++i) {
        if(arr[i][1] > TEST_SIZE * 3 / 5) {
            printf("%d-%ul\t", i, arr[i][1] - arr[i][0]);
        }
    }
    return 0;
}

int main() {
    heapTest();
//    controlledTest();
    test1();

    return 0;
}