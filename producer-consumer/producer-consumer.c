#include "producer-consumer.h"
#include <stdio.h>
#include <stdlib.h>


int pcq_create(pc_queue_t *queue, size_t capacity) {
    int result = 0;

    queue->pcq_buffer = (void **)malloc(capacity * sizeof(void *));
    if (queue->pcq_buffer == NULL) {
        return -1;
    }

    queue->pcq_capacity = capacity;
    queue->pcq_current_size = 0;
    queue->pcq_head = 0;
    queue->pcq_tail = 0;

    pthread_mutex_init(&queue->pcq_current_size_lock, NULL);
    pthread_mutex_init(&queue->pcq_head_lock, NULL);
    pthread_mutex_init(&queue->pcq_tail_lock, NULL);
    pthread_mutex_init(&queue->pcq_pusher_condvar_lock, NULL);
    pthread_mutex_init(&queue->pcq_popper_condvar_lock, NULL);
    pthread_cond_init(&queue->pcq_pusher_condvar, NULL);
    pthread_cond_init(&queue->pcq_popper_condvar, NULL);
    return result;
}


int pcq_destroy(pc_queue_t *queue) {
    int res;
    if ((res = pthread_mutex_destroy(&queue->pcq_current_size_lock)) != 0) return res;
    if ((res = pthread_mutex_destroy(&queue->pcq_head_lock)) != 0) return res;
    if ((res = pthread_mutex_destroy(&queue->pcq_tail_lock)) != 0) return res;
    if ((res = pthread_mutex_destroy(&queue->pcq_pusher_condvar_lock)) != 0) return res;
    if ((res = pthread_mutex_destroy(&queue->pcq_popper_condvar_lock)) != 0) return res;
    if ((res = pthread_cond_destroy(&queue->pcq_pusher_condvar)) != 0) return res;
    if ((res = pthread_cond_destroy(&queue->pcq_popper_condvar)) != 0) return res;
    free(queue->pcq_buffer);
    queue->pcq_capacity = 0;
    queue->pcq_current_size = 0;
    queue->pcq_head = 0;
    queue->pcq_tail = 0;
    return 0;
}



int pcq_enqueue(pc_queue_t *queue, void *elem) {
    // Acquire locks
    pthread_mutex_lock(&queue->pcq_current_size_lock);
    pthread_mutex_lock(&queue->pcq_tail_lock);
    pthread_mutex_lock(&queue->pcq_pusher_condvar_lock);

    // Wait until the queue has space
    while (queue->pcq_current_size >= queue->pcq_capacity) {
        pthread_cond_wait(&queue->pcq_pusher_condvar, &queue->pcq_pusher_condvar_lock);
    }

    // Insert the element at the tail of the queue
    queue->pcq_buffer[queue->pcq_tail] = elem;

    // Increase the tail and size of the queue
    queue->pcq_tail = (queue->pcq_tail + 1) % queue->pcq_capacity;
    queue->pcq_current_size++;

    // Notify waiting threads
    pthread_cond_signal(&queue->pcq_popper_condvar);

    // Release locks
    pthread_mutex_unlock(&queue->pcq_pusher_condvar_lock);
    pthread_mutex_unlock(&queue->pcq_tail_lock);
    pthread_mutex_unlock(&queue->pcq_current_size_lock);

    return 0; // success
}


void *pcq_dequeue(pc_queue_t *queue) {
    void *elem;

    pthread_mutex_lock(&queue->pcq_current_size_lock);
    while (queue->pcq_current_size == 0) {
        pthread_cond_wait(&queue->pcq_popper_condvar, &queue->pcq_current_size_lock);
    }

    pthread_mutex_lock(&queue->pcq_tail_lock);
    elem = queue->pcq_buffer[queue->pcq_tail];
    queue->pcq_tail = (queue->pcq_tail + 1) % queue->pcq_capacity;
    queue->pcq_current_size--;
    pthread_mutex_unlock(&queue->pcq_tail_lock);

    pthread_mutex_unlock(&queue->pcq_current_size_lock);
    pthread_cond_signal(&queue->pcq_pusher_condvar);

    return elem;
}


