#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
// #include <stdatomic.h>

// atomic_int atomicCounter = 0;

typedef enum
{
    PROGRAMMING,
    WAITING,
    BEING_TUTORED
} StudentState;

struct student
{
    unsigned long id;
    unsigned long tutor_id;
    int help;
    StudentState state;
    pthread_mutex_t mutex;
};

struct student_wait_buffer
{
    int size;
    struct student **arr;
    int *is_open;
    int open_positions;
    pthread_mutex_t *lock;
};

struct student_wait_buffer *initialize_wait_buffer(int size)
{
    struct student_wait_buffer *buffer = (struct student_wait_buffer *)malloc(sizeof(struct student_wait_buffer));
    buffer->size = size;
    buffer->arr = (struct student **)malloc(size * sizeof(struct student *));
    buffer->is_open = (int *)malloc(size * sizeof(int));
    buffer->lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    int i = 0;
    for (i = 0; i < size; i++)
    {
        buffer->is_open[i] = 1;
    }
    buffer->open_positions = size;
    pthread_mutex_init(buffer->lock, NULL);
    return buffer;
}

void destroy_wait_buffer(struct student_wait_buffer *buffer)
{
    if (buffer == NULL)
    {
        return;
    }
    // Lock the mutex before starting destruction
    pthread_mutex_lock(buffer->lock);
    // Destroy the mutex
    pthread_mutex_destroy(buffer->lock);
    // Free the dynamic memory
    free(buffer->arr);
    free(buffer->is_open);
    free(buffer->lock);
    // Free the buffer itself
    free(buffer);
}

// returns 1 when successful else 0
int insert(struct student_wait_buffer *buffer, struct student *input_student)
{
    int is_success = 0;
    pthread_mutex_lock(buffer->lock);
    // find the open position
    if (buffer->open_positions > 0)
    {
        int i;
        // linear search
        for (i = 0; (i < buffer->size) && (buffer->is_open[i] == 0) && (is_success == 0); i++)
        {
            if (buffer->is_open[i] == 1)
            {
                buffer->arr[i] = input_student;
                buffer->is_open[i] = 0;
                buffer->open_positions = buffer->open_positions - 1;
                is_success = 1;
            }
        }
    }
    pthread_mutex_unlock(buffer->lock);
    return is_success;
}

struct student *pop(struct student_wait_buffer *buffer)
{
    struct student *next_std_ptr = NULL;
    pthread_mutex_lock(buffer->lock);
    if (buffer->open_positions < buffer->size)
    {
        int i;
        int min_help = INT32_MAX;
        int pick = -1;
        for (i = 0; i < buffer->size; i++)
        {
            if (buffer->is_open[i] == 0 && (min_help > buffer->arr[i]->help))
            {
                min_help = buffer->arr[i]->help;
                pick = i;
            }
        }
        if (pick != -1)
        {
            next_std_ptr = buffer->arr[pick];
            buffer->is_open[pick] = 1;
            buffer->open_positions = buffer->open_positions + 1;
        }
    }
    else
    {
        printf("pop open_positions:%d\n", buffer->open_positions);
    }
    pthread_mutex_unlock(buffer->lock);
    return next_std_ptr;
}

/**
 * while(help<help_max){
 * sem_wait(chair_available)
 * insert(inQ)
 * sem_post(chair_occupied)
 * sem_wait(get_tutored)
 * go to programming
 * }
 */
struct student_thread_params
{
    struct student_wait_buffer *buffer;
    sem_t *chair_available;
    sem_t *chair_occupied;
    pthread_mutex_t *counter_mutex;
    int *st_threads_left;
    int help_max;
};
void *student_thread(void *args)
{
    struct student_thread_params *params = (struct student_thread_params *)args;
    struct student *st = (struct student *)malloc(sizeof(struct student));
    st->id = (unsigned long)pthread_self();
    st->help = 0;
    st->state = PROGRAMMING;
    st->tutor_id = -1;
    while (st->help < params->help_max)
    {
        sem_wait(params->chair_available);
        int insert_success = insert(params->buffer, st);
        if (insert_success)
        {
            sem_post(params->chair_occupied);
            // atomic_fetch_add(&atomicCounter, 1);
            // printf("S: %d\n", atomic_load(&atomicCounter));
            printf("S: Student %lu takes a seat. Empty chairs = %d\n", st->id, params->buffer->open_positions);
            st->state = WAITING;
            while (st->state == WAITING)
            {
                usleep(200);
            }
            while (st->state == BEING_TUTORED)
            {
                usleep(200);
            }
            printf("S: Student %lu received help from Tutor %lu.\n", st->id, st->tutor_id);
            st->tutor_id = -1;
        }
        // else
        // {
        //     printf("S: Student %lu found no empty chair,Will try later\n", st->id);
        // }
        // go back to programming
        usleep(2000);
    }
    pthread_mutex_lock(params->counter_mutex);
    (*params->st_threads_left)--;
    pthread_mutex_unlock(params->counter_mutex);
    return NULL;
}

/*
 * sem_wait(chair_occupied)
 * pop(fromQ)
 * sem_post(chair_available)
 * increament(help)
 * sem_post(get_tutored)
 * sem_post(tutor_available)
 */

struct tutor_thread_params
{
    struct student_wait_buffer *buffer;
    sem_t *chair_available;
    sem_t *chair_occupied;
    pthread_mutex_t *counter_mutex;
    int *st_threads_left;
};

void *tutor_thread(void *args)
{
    struct tutor_thread_params *params = (struct tutor_thread_params *)args;
    unsigned long thread_id = (unsigned long)pthread_self();
    while ((*params->st_threads_left))
    {
        sem_wait(params->chair_occupied);
        // atomic_fetch_sub(&atomicCounter, 1);
        // printf("T: %d\n", atomic_load(&atomicCounter));
        // if(atomic_load(&atomicCounter) < -1){
        //     exit(0);
        // }
        struct student *next_st = pop(params->buffer);
        if (next_st != NULL)
        {
            sem_post(params->chair_available);
            next_st->state = BEING_TUTORED;
            next_st->help = next_st->help + 1;
            next_st->tutor_id = thread_id;
            printf("T: Student %lu tutored by %lu\n", next_st->id, thread_id);
            usleep(200);                  // teach for 0.2ms;
            next_st->state = PROGRAMMING; // need a mutex ??
        }
    }
    return NULL;
}
void simulatecsms(int students, int tutors, int chairs, int helplimit)
{
    sem_t chair_available; //= sem_open("chair_available", O_CREAT | O_EXCL, 0644, chairs);
    sem_t chair_occupied;
    sem_init(&chair_occupied,0,0);
    sem_init(&chair_available, 0, chairs);

    pthread_mutex_t st_countex_mutex = PTHREAD_MUTEX_INITIALIZER;
    int st_threads_left = students;
    struct student_wait_buffer *swb = initialize_wait_buffer(chairs);
    struct student_thread_params st_params = {swb, &chair_available, &chair_occupied, &st_countex_mutex, &st_threads_left, helplimit};
    struct tutor_thread_params tt_params = {swb, &chair_available, &chair_occupied, &st_countex_mutex, &st_threads_left};
    pthread_t student_threads[students];
    pthread_t tutor_threads[tutors];
    int i;
    for (i = 0; i < students; i++)
    {
        pthread_create(&student_threads[i], NULL, student_thread, (void *)&st_params);
    }
    for (i = 0; i < tutors; i++)
    {
        pthread_create(&tutor_threads[i], NULL, tutor_thread, (void *)&tt_params);
    }

    for (i = 0; i < students; i++)
    {
        pthread_join(student_threads[i], NULL);
    }
    for (i = 0; i < tutors; i++)
    {
        pthread_join(tutor_threads[i], NULL);
    }
    destroy_wait_buffer(swb);
    sem_destroy(&chair_occupied);
    sem_destroy(&chair_available);
    // sem_unlink("chair_occupied");
    // sem_unlink("chair_available");
}
int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("needs 4 args students, tutors, chairs, helplimit");
        exit(-1);
    }
    simulatecsms(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    return 0;
}