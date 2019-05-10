
#include <stdio.h>      // printf
#include <stdlib.h>     
#include <string.h>     // strlen
#include <math.h>       // rand
#include <time.h>       // clock

#include "cms.h"

#define AMOUNT 1000000
#define STAT_SIZE (AMOUNT / 10)

int statisticsTest(double width, int depth);

int main(int argc, void*argv[]) {

    if(argc % 2 == 0 && argc != 1) {
        printf("Invalid numbet of inputs\n");
        return 1;
    }
    double width = exp(1);
    int depth = 5;

    if(argc == 1) statisticsTest(width, depth);


    for(int i = 0; i < argc / 2; ++i) {
        width = atof(argv[1 + 2 * i]);
        depth = atoi(argv[2 + 2 * i]);
        statisticsTest(width, depth);
    }

    
    return 0;
}

static double getMean(int *errArray, int errNum, int size) {
    double sum = 0;
    for(int i = 0; i < errNum; ++i) {
        sum += errArray[i];
    }
    return sum / size;
}

static double getStdDev(int *errArray, int errNum, int size) {
    double sum = 0;
    for(int i = 0; i < errNum; ++i) {
        sum += pow(errArray[i], 2);
    }
    return sqrt(sum) / size;
}

int statisticsTest(double width, int depth) {
    char str[20];
    int *srcArray = calloc(AMOUNT, sizeof(int));
    int *errArray= calloc(AMOUNT, sizeof(int));
    double resArray[AMOUNT/STAT_SIZE][2];
    CMSketch * cms = NewCMSketch(AMOUNT * width, depth);

    srand(0);
    for(int i = 0; i < AMOUNT; ++i) {
        srcArray[i] = rand() % 1000;
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
        printf("%d itirations with %d errors, MEAN %f, STDEV is %f\n", 
                        (i + 1) * STAT_SIZE, errIdx, resArray[i][0], resArray[i][1]);
    }

    CMS_Destroy(cms);
    free(srcArray);
    free(errArray);

    return 0;
}