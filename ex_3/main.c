//
//  main.c
//  ex_3
//
//  Created by Eliyah Weinberg on 18.12.2017.
//  Copyright © 2017 Eliyah Weinberg. All rights reserved.
//

#include <stdio.h>
#include "threadpool.h"

int foo (void* data){
    char* a = (char*)data;
    printf("thread prints: %c\n", *a);
    return 0;
}

int main(int argc, const char * argv[]) {
    threadpool* tp = create_threadpool(30);
    int i;
    char arr[10];
    for (i=0; i<10; i++)
        arr[i] = 'a'+i;
    for (i=0; i<10; i++) {
        dispatch(tp, foo, &arr[i]);
    }
    destroy_threadpool(tp);
    return 0;
}
