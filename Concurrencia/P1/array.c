// dario.santos / oscar.olveira

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "options.h"

#define DELAY_SCALE 1000
#define MAX_WAIT 10000

struct array {
    int size;
    int *arr;
    pthread_mutex_t *mutex;
};


struct thread_data{
    int id;
    int iterations;
    int delay;
    struct array *arr;
    struct globalIter *global_iter;
    struct globalIter *global_iter2;
};


struct globalIter{
    int globalIterations;
    pthread_mutex_t *mutex_globalIter;
};


void apply_delay(int delay) {
    for(int i = 0; i < delay * DELAY_SCALE; i++); // waste time
}


void *increment_thread(void *args){
    struct thread_data *td;
    int pos, val;

    td=(struct thread_data *)args;

    int id=td->id;
    int iterations=td->iterations;
    int delay=td->delay;
    struct array *arr=td->arr;
    struct globalIter *global_iter=td->global_iter; 

    for(int i = 0; i < iterations; i++) {
        pos = rand() % arr->size;

        pthread_mutex_lock(global_iter->mutex_globalIter);
        if(global_iter->globalIterations==iterations){
            pthread_mutex_unlock(global_iter->mutex_globalIter);
            return NULL;
        } 
        else global_iter->globalIterations++;
        pthread_mutex_unlock(global_iter->mutex_globalIter);

        pthread_mutex_lock(&arr->mutex[pos]);

        printf("%d increasing position %d\n", id, pos);

        val = arr->arr[pos];
        apply_delay(delay);

        val ++;
        apply_delay(delay);

        arr->arr[pos] = val;
        apply_delay(delay);
                
        pthread_mutex_unlock(&arr->mutex[pos]);
    }
    return NULL;
}


void *increment_thread2(void *args){
    struct thread_data *td;
    int pos, pos2, val;

    td=(struct thread_data *)args;

    int id=td->id;
    int iterations=td->iterations;
    int delay=td->delay;
    struct array *arr=td->arr;
    struct globalIter *global_iter2=td->global_iter2; 

    for(int i = 0; i < iterations; i++) {
        pos = rand() % arr->size;
        do{
            pos2 = rand() % arr->size;
        }while(pos2==pos);

        pthread_mutex_lock(global_iter2->mutex_globalIter);
        if(global_iter2->globalIterations==iterations){
            pthread_mutex_unlock(global_iter2->mutex_globalIter);
            return NULL;
        }
        else global_iter2->globalIterations++;
        pthread_mutex_unlock(global_iter2->mutex_globalIter);


        while(1){
            if(pthread_mutex_trylock(&arr->mutex[pos])==0){
                if(pthread_mutex_trylock(&arr->mutex[pos2])==0){

                    printf("%d increasing position %d\n", id, pos);

                    val = arr->arr[pos];
                    apply_delay(delay);

                    val ++;
                    apply_delay(delay);

                    arr->arr[pos] = val;
                    apply_delay(delay);

                    printf("%d decreasing position %d\n", id, pos2);

                    val = arr->arr[pos2];
                    apply_delay(delay);

                    val --;
                    apply_delay(delay);

                    arr->arr[pos2] = val;
                    apply_delay(delay);


                    pthread_mutex_unlock(&arr->mutex[pos]);
                    pthread_mutex_unlock(&arr->mutex[pos2]);  
                    break;              
                }
                else pthread_mutex_unlock(&arr->mutex[pos]);
                usleep(rand() % MAX_WAIT);
            }
        }
                
    }
    return NULL;
}


void print_array(struct array arr) {
    int total = 0;

    for(int i = 0; i < arr.size; i++) {
        total += arr.arr[i];
        printf("%d ", arr.arr[i]);
    }

    printf("\nTotal: %d\n", total);
}


int main (int argc, char **argv)
{
    struct options       opt;
    struct array         arr;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 5;
    opt.size         = 10;
    opt.iterations   = 5;
    opt.delay        = 1000;

    read_options(argc, argv, &opt);

    arr.size = opt.size;
    arr.arr  = malloc(arr.size * sizeof(int));

    arr.mutex=malloc(arr.size * sizeof(pthread_mutex_t));

    for (int i = 0; i<arr.size; i++){
        pthread_mutex_init(&arr.mutex[i], NULL);
    }

    memset(arr.arr, 0, arr.size * sizeof(int));

    pthread_t *threads;
    pthread_t *threads2;
    struct thread_data *thread_args;
    threads=malloc(opt.num_threads * sizeof(pthread_t));
    threads2=malloc(opt.num_threads * sizeof(pthread_t));
    thread_args=malloc(opt.num_threads * sizeof(struct thread_data));

    struct globalIter global_iter;
    global_iter.globalIterations=0;
    global_iter.mutex_globalIter=malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(global_iter.mutex_globalIter, NULL);

    struct globalIter global_iter2;
    global_iter2.globalIterations=0;
    global_iter2.mutex_globalIter=malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(global_iter2.mutex_globalIter, NULL);

    for(int i=0; i<opt.num_threads;i++){
        thread_args[i].id=i;
        thread_args[i].iterations=opt.iterations;
        thread_args[i].delay=opt.delay;
        thread_args[i].arr=&arr;
        thread_args[i].global_iter=&global_iter;
        thread_args[i].global_iter2=&global_iter2;

        pthread_create(&threads[i], NULL, increment_thread, &thread_args[i]);
    }

    for(int i=0; i<opt.num_threads;i++){
        thread_args[i].id=i;
        thread_args[i].iterations=opt.iterations;
        thread_args[i].delay=opt.delay;
        thread_args[i].arr=&arr;
        thread_args[i].global_iter=&global_iter;
        thread_args[i].global_iter2=&global_iter2;

        pthread_create(&threads2[i], NULL, increment_thread2, &thread_args[i]);
    }

    for(int i=0; i<opt.num_threads; i++){
        pthread_join(threads[i], NULL);
    }

    for(int i=0; i<opt.num_threads; i++){
        pthread_join(threads2[i], NULL);
    }

    print_array(arr);

    for (int i = 0; i<arr.size; i++){
        pthread_mutex_destroy(&arr.mutex[i]);
    }

    pthread_mutex_destroy(global_iter.mutex_globalIter);
    pthread_mutex_destroy(global_iter2.mutex_globalIter);
    
    free(threads);
    free(threads2);
    free(thread_args);
    free(arr.mutex);
    free(arr.arr);
    free(global_iter.mutex_globalIter);
    free(global_iter2.mutex_globalIter);
    pthread_exit(NULL);
    exit(0);
}
