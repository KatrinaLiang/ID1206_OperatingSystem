/*
In this program we will have two threads incrementing a shared counter. 
Use futex to implement futex locks, futex_wait() and gutex_wake().
*/
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <linux/futex.h>
#include <sys/syscall.h>

volatile int count = 0;

volatile int global = 0;

typedef struct args {
  int inc;
  int id;
  volatile int *mutex;
} args;

// we look at the address provided by futexp and if it is equal to 1 we suspend.
int futex_wait(volatile int *futexp){
  return syscall(SYS_futex, futexp, FUTEX_WAIT, 1, NULL, NULL, 0);
}

// we will wake at most 1 thread that is suspended on the futex address. 
void futex_wake(volatile int *futexp){
  syscall(SYS_futex, futexp, FUTEX_WAKE, 1, NULL, NULL, 0);
}

// atomic
int try(volatile int *mutex){
  return __sync_val_compare_and_swap(mutex, 0, 1);
}

int lock(volatile int *lock){
  int susp = 0;
  while(try(lock) != 0){ 
    susp++;
    futex_wait(lock);
  }
  return susp;
}

void unlock(volatile int *lock){
  *lock = 0;
  futex_wake(lock);
}


void *increment(void *arg){
  int inc = ((args*)arg)->inc;
  int id = ((args*)arg)->id;
  volatile int *mutex = ((args*)arg)->mutex;
  
  printf("start %d\n", id);
  
  int spin = 0;
  
  for(int i = 0; i < inc; i++){
    spin += lock(mutex);
    count++;
    unlock(mutex);
  }
  printf("stop %d, spining %d\n", id, spin);  
}

int main(int argc, char *argv[]){
  if(argc != 2){
    printf("usage futex <inc>\n");
    exit(0);
  }
  
  int inc = atoi(argv[1]);
  
  pthread_t one_p, two_p;
  args one_args, two_args;
  
  one_args.inc = inc;
  two_args.inc = inc;
  
  one_args.id = 0;
  two_args.id = 1;
  
  one_args.mutex = &global;
  two_args.mutex = &global;
  
  pthread_create(&one_p, NULL, increment, &one_args);
  pthread_create(&two_p, NULL, increment, &two_args);
  pthread_join(one_p, NULL);
  pthread_join(two_p, NULL);
  
  printf("result is %d\n", count);
  return 0;
}






