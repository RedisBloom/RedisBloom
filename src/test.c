
#include <stdio.h>

#include "cms.h"

int visualTest();

int main()
{
    visualTest();

    return 0;
}

int visualTest() {
    CMSketch *a = NewCMSketch(20, 5);
    CMSketch *b = NewCMSketch(20, 5);

    CMS_IncrBy(a, "C", 1);
    CMS_Print(a);
    CMS_IncrBy(a, "C", 15);
    CMS_Print(a);
    CMS_IncrBy(b, "C", 100);
    CMS_Print(b);
    CMS_IncrBy(b, "M", 35);
    CMS_Print(b);
    CMS_IncrBy(b, "S", 87);
    CMS_Print(b);
    CMS_IncrBy(b, "CMS", 12);
    CMS_Print(b);
    
    printf("C count at a is %lu\n",CMS_Query(a, "C"));
    printf("C count at b is %lu\n",CMS_Query(b, "C"));
    printf("M count at b is %lu\n",CMS_Query(b, "M"));
    printf("S count at b is %lu\n",CMS_Query(b, "S"));
    printf("CMS count at b is %lu\n",CMS_Query(b, "CMS"));

    CMSketch *c = NewCMSketch(20, 5);
    CMSketch *list[2] = { a, b };
    long long weight[2] = { 3, 2 };
    CMS_Merge(c, 2, list, weight);
    printf("q %lu\n", CMS_Query(a, "C"));
    printf("q %lu\n", CMS_Query(b, "C"));
    printf("q %lu\n", CMS_Query(c, "C"));
    CMS_Print(c);

    CMS_Destroy(a);
    CMS_Destroy(b);
    CMS_Destroy(c);
    return 0;
}