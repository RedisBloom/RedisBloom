#include "vector.h"
#include <stdio.h>


int __vector_PushPtr(Vector *v, void *elem) {
    if (v->top == v->cap - 1) {
        Vector_Resize(v, v->cap ? v->cap*2 : 1);
    }
    
    __vector_PutPtr(v, v->top, elem);
    v->top++;
    return v->top;
}


int Vector_Get(Vector *v, size_t pos, void *ptr) {
    // return 0 if pos is out of bounds
    if (pos >= v->top) {
        return 0;
    }
    
    memcpy(ptr, v->data + (pos * v->elemSize), v->elemSize);
    return 1;
}


int __vector_PutPtr(Vector *v, size_t pos, void *elem) {
    // resize if pos is out of bounds
    if (pos >= v->cap) {
        Vector_Resize(v, pos+1);
    }
    
    memcpy(v->data + pos*v->elemSize, elem, v->elemSize);
    // move the end offset to pos if we grew
    if (pos > v->top) {
        v->top = pos;
    }
    return 1;
}


int Vector_Resize(Vector *v, size_t newcap) {
    
    int oldcap = v->cap;
    v->cap = newcap;
    
    v->data = realloc(v->data, v->cap*v->elemSize);
    
    // If we grew:
    // put all zeros at the newly realloc'd part of the vector
    if (newcap > oldcap) {
       int offset = oldcap*v->elemSize;
       memset(v->data + offset, 0, v->cap*v->elemSize - offset);
    }
    return v->cap;
}

inline int Vector_Size(Vector *v) {
    return v->top;
}

inline int Vector_Cap(Vector *v) {
    return v->cap;
}

Vector *__newVectorSize(size_t elemSize, size_t cap) {
    
    Vector *vec = malloc(sizeof(Vector));
    vec->data = calloc(cap, elemSize);
    vec->top = 0;
    vec->elemSize = elemSize;
    vec->cap = cap;
   
    return vec;  
}

void Vector_Free(Vector *v) {
    free(v->data);
    free(v);
}
