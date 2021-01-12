//
// Created by katrina on 2021-01-09.
//

#include <stdio.h>
#include "green.h"

#define INC 1000000

int flag = 0;
green_cond_t cond;
green_mutex_t lock;
volatile int count;

void *test(void *arg){
    int id = *(int*)arg;
    printf("start %d\n", id);
    for(int i = 0; i < INC; i++){
        green_mutex_lock(&lock);
        count++;
        green_mutex_unlock(&lock);
    }
    printf("stop %d\n", id);
}

int main(){
    green_t g0, g1;
    int a0 = 0;
    int a1 = 1;

    green_mutex_init(&lock);

    green_create(&g0, test, &a0);
    green_create(&g1, test, &a1);
    green_join(&g0, NULL);
    green_join(&g1, NULL);

    printf("result is %d\n", count);
    printf("done\n");
    return 0;
}
