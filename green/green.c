//
// Created by katrina on 2021-01-03.
//
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>
#include "green.h"

#define FALSE 0
#define TRUE 1

#define STACK_SIZE 4096
#define PERIOD 100

static ucontext_t main_cntx = {0};
/* The global green thread that holds a pointer to the main context*/
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, NULL, FALSE};
static green_t *running = &main_green;
static green_t *ready = NULL;
static green_t *ready_end = NULL;

static sigset_t block;

void add_to_ready(green_t **queue, green_t **end, green_t *new);
void enqueue(green_t **queue, green_t *new);
green_t * dequeue(green_t **);
void timer_handler(int);

static void init() __attribute__((constructor));

void init(){
    getcontext(&main_cntx);

    sigemptyset(&block);
    sigaddset(&block, SIGVTALRM);

    struct sigaction act = {0};
    struct timeval interval;
    struct itimerval period;

    act.sa_handler = timer_handler;
    assert(sigaction(SIGVTALRM, &act, NULL) == 0);

    interval.tv_sec = 0;
    interval.tv_usec = PERIOD;
    period.it_interval = interval;
    period.it_value = interval;
    setitimer(ITIMER_VIRTUAL, &period, NULL);
}

void timer_handler(int sig){
    sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *susp = running;
    //add_to_ready(&ready, &ready_end, susp);
    enqueue(&ready, susp);
    green_t *next = dequeue(&ready);
    running = next;

    sigprocmask(SIG_UNBLOCK, &block, NULL);
    swapcontext(susp->context, next->context);
}
/******* create a new green thread *******/
/*
 * The user will first call green_create() and provide it an uninitialized green_t structure,
 * the function the thread should execute and pointer to its arguments.
 * A new context is created and attached to the thread structure and add this thread to the ready queue.
 * Then call green_thread() which has the responsible for calling the function provided by the user.
 *
 * green_thread() start the execution of the real function and,
 * when after returning from the call, terminate the thread.
 */
void green_thread(){
    green_t *this = running;
    (*this->fun)(this->arg); // result

    // place waiting (joining) thread in ready queue
    if(this->join != NULL){
        //add_to_ready(&ready, &ready_end, this->join);
        enqueue(&ready,this->join);
    }

    //save result of execution

    // we're a zombie
    this->zombie = TRUE;

    // find the next thread to run
    green_t *next = dequeue(&ready);

    running = next;
    setcontext(next->context);
}

int green_create(green_t *new, void *(*fun)(void*), void *arg){
    //printf("green_create BEGIN\n");
    ucontext_t *cntx = (ucontext_t *)malloc(sizeof(ucontext_t));
    getcontext(cntx);

    void *stack = malloc(STACK_SIZE);

    cntx->uc_stack.ss_sp = stack;
    cntx->uc_stack.ss_size = STACK_SIZE;
    makecontext(cntx, green_thread, 0);

    new->context = cntx;
    new->fun = fun;
    new->arg = arg;
    new->next = NULL;
    new->join = NULL;
    new->retval = NULL;
    new->zombie = FALSE;

    // add new to the ready queue
    //add_to_ready(&ready, &ready_end, new);
    enqueue(&ready, new);
    //printf("green_create\n");
    return 0;
}

/*void add_to_ready(green_t **queue, green_t *new){
    if(queue == NULL){
        queue = new;
    }else{
        green_t *pointer = queue;
        while(pointer->next != NULL){
            pointer = pointer->next;
        }
        pointer->next = new;
    }
}*/
void add_to_ready(green_t **front, green_t **end, green_t *new){
    //printf("add to ready done BEGIN\n");
    if(*front == NULL && *end == NULL){
        *front = new;
        *end = new;
    } else{
        green_t *p = *end;
        p->next = new;
        *end = new;
    }
    //printf("add to ready done\n");
}

green_t *dequeue(green_t **front){
    if(*front == NULL){
        return NULL;
    } else {
        green_t *p = *front;
        *front = (*front)->next;
        p->next = NULL;
        return p;
    }
}

/*** yield the execution ***/
int green_yield(){
    green_t *susp = running;
    // add susp to ready queue
    //add_to_ready(&ready, &ready_end, susp);
    enqueue(&ready, susp);

    // select the next thread for execution
    green_t *next = dequeue(&ready);
    running = next;
    swapcontext(susp->context, next->context);
    return 0;
}

/*** wait for a thread to terminate ***/
int green_join(green_t *thread, void **res){
    //printf("green_join BEGIN\n");
    if(!thread->zombie) {
        green_t *susp = running;
        sigprocmask(SIG_BLOCK, &block, NULL);

        // add to waiting threads
        if (thread->join == NULL) {
            thread->join = susp; // If no thread joining, just put it in join field
        } else {
            green_t *current = thread->join; // Otherwise, find tail of queue, and add last
            while (current->next != NULL)
                current = current->next;
            current->next = susp;
        }
        // select the next thread for execution
        green_t *next = dequeue(&ready);
        running = next;
        swapcontext(susp->context, next->context);
        sigprocmask(SIG_UNBLOCK, &block, NULL);
    }
    // collect result ?????????
    res = thread->retval;

    // free context
    free(thread->context->uc_stack.ss_sp);
    free(thread->context);

    return 0;
}
/*** Conditional variables ***/
void green_cond_init(green_cond_t *cond){ // initialize a green cond vari
    cond->list = NULL;
}

void enqueue(green_t **list, green_t *new){
    if(*list == NULL){
        *list = new;
    }else{
        green_t *current = *list;
        while(current->next != NULL){
            current = current->next;
        }
        current->next = new;
    }
}

void green_cond_wait(green_cond_t *cond,green_mutex_t *mutex){ // suspend the current thread on the condition
    //printf("green_cond_wait BEGIN\n");
    //sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *susp = running;
    // add to the suspend list
    enqueue(&cond->list, susp);
    // run another thread
    green_t *next = dequeue(&ready);
    running = next;

    //sigprocmask(SIG_BLOCK, &block, NULL);
    swapcontext(susp->context, next->context);
    //printf("green_cond_wait\n");

}

void green_cond_signal(green_cond_t *cond){ // move the first suspended thread to the ready queue
    green_t *thread = cond->list;
    if(thread != NULL){
        cond->list = thread->next;
        thread->next = NULL;
        //add_to_ready(&ready, &ready_end, thread);
        enqueue(&ready, thread);
    }
}

/*** Mutex lock ***/
int green_mutex_init(green_mutex_t *mutex){
    mutex->taken = FALSE;
    mutex->susp = NULL;
}

int green_mutex_lock(green_mutex_t *mutex){
    // block timer interrupt
    sigprocmask(SIG_BLOCK, &block, NULL);

    green_t *susp = running;
    if(mutex->taken){
        // suspend the running thread
        enqueue(&ready, susp);
        // select the next thread for execution
        green_t *next = dequeue(&ready);
        running = next;
        swapcontext(susp->context, next->context);
    } else{
        // take the lock
        mutex->taken = TRUE;
    }
    // unblock
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
}

int green_mutex_unlock(green_mutex_t *mutex){
    // block timer interrupt
    sigprocmask(SIG_BLOCK, &block, NULL);

    if(mutex->susp != NULL){
        // move suspended thread to ready queue
        green_t *thread = mutex->susp;
        enqueue(&ready, thread);
        mutex->susp = thread->next;
        thread->next = NULL;
    } else{
        // release lock
        mutex->taken = FALSE;
        mutex->susp = NULL;
    }
    // unblock
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
}