//
// Created by katrina on 2021-01-09.
//

#include <stdio.h>
#include "green.h"

int flag = 0;
green_cond_t cond;
green_mutex_t mutex;
int count = 0;
#define LOOP 10000

void *test(void *arg){
    int id = *(int*)arg;
    int loop = LOOP;
    while(loop > 0){
        green_mutex_lock(&mutex);
        while(flag != id){
            //printf("thread %d: %d\n", id, loop);
            green_mutex_unlock(&mutex);
            green_cond_wait(&cond, &mutex);
        }
        flag = (id +1) % 2;
        count++;
        green_cond_signal(&cond);
        green_mutex_unlock(&mutex);
        loop--;
    }
}

int main(){
    green_t g0, g1;
    int a0 = 0;
    int a1 = 1;

    green_cond_init(&cond);

    green_create(&g0, test, &a0);
    green_create(&g1, test, &a1);

    green_join(&g0, NULL);
    green_join(&g1, NULL);

    printf("Result: %d\n", count);
    printf("done\n");
    return 0;
}
