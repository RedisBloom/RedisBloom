/* Compile using  
 * gcc test_cms.c  -I../  -I../src ../src/cms.c ../contrib/MurmurHash2.c -lm 
 */

#include <stdio.h> // printf
#include <stdlib.h>
#include <string.h> // strlen
#include <math.h>   // rand
#include <time.h>   // clock

#include "cms.h"

#define AMOUNT 10000
#define STAT_SIZE (AMOUNT / 10)

int visualTest();
int massiveTest();

int main() {
    visualTest();
    massiveTest();

    return 0;
}

int visualTest() {
    CMSketch *a = NewCMSketch(20, 5);
    CMSketch *b = NewCMSketch(20, 5);

    CMS_IncrBy(a, "C", 1, 1);
    CMS_Print(a);
    CMS_IncrBy(a, "C", 1, 15);
    CMS_Print(a);
    CMS_IncrBy(b, "C", 1, 100);
    CMS_Print(b);
    CMS_IncrBy(b, "M", 1, 35);
    CMS_Print(b);
    CMS_IncrBy(b, "S", 1, 87);
    CMS_Print(b);
    CMS_IncrBy(b, "CMS", 3, 12);
    CMS_Print(b);

    printf("C count at a is %lu\n", CMS_Query(a, "C", 1));
    printf("C count at b is %lu\n", CMS_Query(b, "C", 1));
    printf("M count at b is %lu\n", CMS_Query(b, "M", 1));
    printf("S count at b is %lu\n", CMS_Query(b, "S", 1));
    printf("CMS count at b is %lu\n", CMS_Query(b, "CMS", 3));

    CMSketch *c = NewCMSketch(20, 5);
    const CMSketch *list[2] = {a, b};
    long long weight[2] = {3, 2};
    CMS_Merge(c, 2, list, weight);
    printf("q %lu\n", CMS_Query(a, "C", 1));
    printf("q %lu\n", CMS_Query(b, "C", 1));
    printf("q %lu\n", CMS_Query(c, "C", 1));
    CMS_Print(c);

    CMS_Destroy(a);
    CMS_Destroy(b);
    CMS_Destroy(c);
    return 0;
}

int massiveTest() {
    CMSketch *cms = NewCMSketch(AMOUNT * 2.7, 5);
    int original[AMOUNT] = {0};
    int result[AMOUNT] = {0};
    int totalErr = 0;

    srand(0);
    char str[20];
    for (int i = 0; i < AMOUNT; ++i) {
        original[i] = rand() % 1000;
    }

    clock_t begin = clock();
    for (int i = 0; i < AMOUNT; ++i) {
        sprintf(str, "%d", i);
        for (int j = 0; j < original[i]; ++j) {
            CMS_IncrBy(cms, str, strlen(str), 1);
        }
    }
    clock_t end = clock();

    for (int i = 0; i < AMOUNT; ++i) {
        sprintf(str, "%d", i);
        size_t res = CMS_Query(cms, str, strlen(str));
        if (res > original[i]) {
            ++totalErr;
        }
    }

    int counter = 0;
    for (int i = 0; i < cms->width * cms->depth; ++i) {
        if (cms->array[i] == 0)
            ++counter;
    }

    printf("counter of 0 is %d\r\n", counter);
    printf("counter of err is %d\r\n", totalErr);
    printf("execution time %f\n", (double)(end - begin) / CLOCKS_PER_SEC);

    return 0;
}