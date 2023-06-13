#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

// circular array
typedef struct _queue {
    int size;
    int used;
    int first;
    void **data;
    pthread_mutex_t *buffer_lock;
    pthread_cond_t *buffer_full;
    pthread_cond_t *buffer_empty;
    bool entries_end;
} _queue;


#include "queue.h"

queue q_create(int size) {
    queue q = malloc(sizeof(_queue));

    q->buffer_lock=malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(q->buffer_lock, NULL);

    q->buffer_full=malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->buffer_full, NULL);

    q->buffer_empty=malloc(sizeof(pthread_cond_t));
    pthread_cond_init(q->buffer_empty, NULL);

    q->size  = size;
    q->used  = 0;
    q->first = 0;
    q->data  = malloc(size * sizeof(void *));
    q->entries_end=false;

    return q;
}

int q_elements(queue q) {
    return q->used;
}

int q_insert(queue q, void *elem) {

    pthread_mutex_lock(q->buffer_lock);
    while(q->used==q->size) { // Esperar por sitio
        pthread_cond_wait(q->buffer_full, q->buffer_lock);
    }

    q->data[(q->first + q->used) % q->size] = elem;
    q->used++;

    if(q->used==1){
        pthread_cond_broadcast(q->buffer_empty);
    }
    pthread_mutex_unlock(q->buffer_lock);
    
    return 0;
}

void *q_remove(queue q) {

    pthread_mutex_lock(q->buffer_lock);
    while(q->used==0 && !q->entries_end){
        pthread_cond_wait(q->buffer_empty, q->buffer_lock);
    }
    if(q->entries_end && q->used==0){
        pthread_mutex_unlock(q->buffer_lock);
        return NULL;
    }

    void *res;

    res = q->data[q->first];

    q->first = (q->first + 1) % q->size;
    q->used--;

    if(q->used==q->size-1)
        pthread_cond_broadcast(q->buffer_full);
    
    pthread_mutex_unlock(q->buffer_lock);

    return res;
}

void q_destroy(queue q) {

    pthread_mutex_destroy(q->buffer_lock);
    pthread_cond_destroy(q->buffer_full);
    pthread_cond_destroy(q->buffer_empty);

    free(q->buffer_lock);
    free(q->buffer_full);
    free(q->buffer_empty);

    free(q->data);
    free(q);
}

void activar_bool(queue q){
    q->entries_end=true;
    pthread_cond_broadcast(q->buffer_empty);
}

bool q_bool(queue q) {
    return q->entries_end;
}