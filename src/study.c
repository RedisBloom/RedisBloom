
#include <stdio.h>      // printf
#include <stdlib.h>     // calloc
#include <string.h>     // strlen
#include <math.h>       // rand
#include <time.h>       // clock

#include "cms.h"

#define AMOUNT 1000000
#define STAT_SIZE (AMOUNT / 10)

int statisticsTest(double width, int depth);

int main(int argc, void*argv[]) {

    double width = 2.7;
    int depth = 5;

    if(argc == 1) {
        statisticsTest(width, depth);
    } else if(argc == 2 && ((char *)argv[1])[0] == 't') {
        clock_t begin = clock();
        for(double width = 1; width <= 4; width += 0.1) {
            for(int depth = 1; depth < 7; ++depth) {
                //printf("width %f, depth %d\n", width, depth);
                statisticsTest(width, depth);
            }
        }
    clock_t end = clock();
    printf("execution time %f\n", (double)(end - begin) / CLOCKS_PER_SEC);
    } else if(argc % 2 == 1) {
        for(int i = 1; i < argc - 1; i += 2) {
            width = atof(argv[i]);
            depth = atoi(argv[i + 1]);
            statisticsTest(width, depth);
        }
    }
    else {
        printf("Invalid numbet of inputs\n");
        return 1;
    }
    
    return 0;
}

static double getMean(int *errArray, int errNum, int size) {
    double sum = 0;
    for(int i = 0; i < errNum; ++i) {
        sum += errArray[i];
    }
    return sum / errNum;
}

static double getStdDev(int *errArray, int errNum, int size) {
    double sum = 0;
    for(int i = 0; i < errNum; ++i) {
        sum += pow(errArray[i], 2);
    }
    return sqrt(sum) / errNum;
}

int statisticsTest(double width, int depth) {
    char str[20];
    int *srcArray = calloc(AMOUNT, sizeof(int));
    int *errArray= calloc(AMOUNT, sizeof(int));
    double resArray[AMOUNT/STAT_SIZE][2];
    CMSketch *cms = NewCMSketch(AMOUNT * width, depth);

    srand(0);
    for(int i = 0; i < AMOUNT; ++i) {
        srcArray[i] = pow(rand() % 100, 2);
    }
    
    int i, j, k, idx = 0, errIdx = 0, resIdx = 0;
    for(i = 0; i < AMOUNT / STAT_SIZE; ++i) {
        for(j = 0; j < STAT_SIZE; ++j) {
            idx = STAT_SIZE * i + j;
            sprintf(str, "%d", idx);
            CMS_IncrBy(cms, str, strlen(str), srcArray[idx]);        
        }

        for(k = 0, errIdx = 0; k < idx; ++k) {
            sprintf(str, "%d", k);
            size_t res = CMS_Query(cms, str, strlen(str));
            if(res > srcArray[k]) {
                errArray[errIdx++] = res - srcArray[k];
            }
        }      

        resArray[i][0] = getMean(errArray, errIdx, idx);
        resArray[i][1] = getStdDev(errArray, errIdx, idx);
        printf("width %f, depth %d, ", width, depth);
        printf("%d itirations, counter %lu\t", (i + 1) * STAT_SIZE, cms->counter);
        printf("with %d errors, MEAN %f, STDEV is %f\t", errIdx, resArray[i][0], resArray[i][1]);
        printf("Cardinality of %lu\n", CMS_cardinality(cms));
    }

    CMS_Destroy(cms);
    free(srcArray);
    free(errArray);

    return 0;
}