#include <stdio.h> // printf
#include <stdlib.h>
#include <string.h> // strlen
#include <math.h>   // rand
#include <time.h>   // clock

#include "topk.h"
#include "topk.c"

#define TEST_SIZE 100
#define PRINT(str) printf("%s\n", str)

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


int test1() {
    int arr[TEST_SIZE][2] = { 0 }, total = 0;
    char str[TEST_SIZE * 10] = { 0 };
    TopK *topk = TopK_Create(7, TEST_SIZE/2, 4);
  //  char str[12] = { 0 };
    char *runner = str;

    PRINT("created");

    srand(0);
    for(int i = 0; i < TEST_SIZE; ++i) {
        arr[i][0] = arr[i][1] = rand() % TEST_SIZE;
        total += arr[i][0];
        sprintf(runner, "%d", i);
        runner +=10;
    }

    PRINT("arr filled");

    for(int i = 0; i < total;) {
        int idx = rand() % TEST_SIZE;
        if(arr[idx][0] > 0) {
            TopK_Add(topk, str + 10 * idx, strlen(str + 10 * idx));
            --arr[idx][0];
            ++i;
        }
    }

    PRINT("added");

    const char **res = TopK_List(topk);
    for(int i = 0; i < topk->k; ++i) {
        if(res[i]) printf("%s\n", res[i]);
    }

    PRINT("print Top K");

    for(int i = 0; i < TEST_SIZE; ++i) {
        if(arr[i][i] > TEST_SIZE * 4 / 5) {
            printf("%d\t", i);
        }
    }
    PRINT("finished");
    return 0;
}

int main() {
    heapTest();
   // test1();

    return 0;
}