#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

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
        for (i = 0; (i < buffer->size) && (is_success == 0); i++)
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
        int min_help = INT_MAX;
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

// struct coordinator_thread_params{
//     struct student_wait_buffer *buffer;
//     sem_t* notify_coorinator;
//     sem_t* notify_student;
//     pthread_mutex_t *next_student_mutex;
//     struct student* next_student;
//     pthread_mutex_t *snapshop_mutex;
//     int *total_requests;
//     int *insert_response;

// }
// void* coordinator_thread(void* args){

// }

struct coordinator_thread_params{
    struct student_wait_buffer* buffer;
    struct student** arrived_student;
    pthread_mutex_t* snapshot_mutex;
    int* total_requests_ptr;
    sem_t* chair_occupied;
    sem_t* student_to_coordinator;
    sem_t* coordinator_to_student;
    int* coordinator_thread_return;
};

void *coordinator_thread(void *args)
{
    while (1)
    {
        struct coordinator_thread_params *params = (struct coordinator_thread_params *)args;
        sem_wait(params->student_to_coordinator);
        int insert_success = insert(params->buffer, *params->arrived_student);
        if (insert_success)
        {
            pthread_mutex_lock(params->snapshot_mutex);
            (*params->total_requests_ptr)++;
            int total_requests = (*params->total_requests_ptr);
            int waiting_students = params->buffer->size - params->buffer->open_positions;
            pthread_mutex_unlock(params->snapshot_mutex);
            sem_post(params->chair_occupied);
            printf("C: Student %lu with priority %d added to the queue. Waiting students now = %d. Total requests = %d\n", (*params->arrived_student)->id, (*params->arrived_student)->help, waiting_students, total_requests);
        }
        (*params->coordinator_thread_return) = insert_success;
        sem_post(params->coordinator_to_student);
    }
};

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
    pthread_mutex_t *snapshop_mutex;
    int *total_requests;
    pthread_mutex_t *coordinator_access;
    sem_t* student_to_coordinator;
    sem_t* coordinator_to_student;
    struct student** coordinator_input;
    int* coordinator_thread_return;
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
        //int insert_success = insert(params->buffer, st);
        pthread_mutex_lock(params->coordinator_access);
        (*params->coordinator_input) = st;
        int empty_chairs = params->buffer->open_positions;
        sem_post(params->student_to_coordinator);
        sem_wait(params->coordinator_to_student);
        int insert_success = (*params->coordinator_thread_return);
        (*params->coordinator_input) = NULL;
        pthread_mutex_unlock(params->coordinator_access);
        //int insert_success =  coordinator(params->buffer,st,params->snapshop_mutex,params->total_requests,params->chair_occupied);
        if (insert_success)
        {
            // pthread_mutex_lock(params->snapshop_mutex);
            // (*params->total_requests)++;
            // int total_requests = (*params->total_requests);
            // int waiting_students = params->buffer->size - params->buffer->open_positions;
            // pthread_mutex_unlock(params->snapshop_mutex);
            // sem_post(params->chair_occupied);
            printf("S: Student %lu takes a seat. Empty chairs = %d\n", st->id, empty_chairs-1);
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
        else
        {
            printf("S: Student %lu found no empty chair,Will try later\n", st->id);
        }
        // go back to programming
        usleep(2000);
    }
    pthread_mutex_lock(params->counter_mutex);
    (*params->st_threads_left)--;
    int threads_left = (*params->st_threads_left);
    pthread_mutex_unlock(params->counter_mutex);
    if(threads_left == 0){
        exit(0);
    }
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
    pthread_mutex_t *snapshop_mutex;
    int *total_sessions;
    int *active_sessions;
};

void *tutor_thread(void *args)
{
    struct tutor_thread_params *params = (struct tutor_thread_params *)args;
    unsigned long thread_id = (unsigned long)pthread_self();
    while (1)
    {
        pthread_mutex_lock(params->counter_mutex);
        if (*params->st_threads_left == 0)
        {
            pthread_mutex_unlock(params->counter_mutex);
            break;
        }
        pthread_mutex_unlock(params->counter_mutex);
        sem_wait(params->chair_occupied);
        struct student *next_st = pop(params->buffer);
        if (next_st != NULL)
        {
            pthread_mutex_lock(params->snapshop_mutex);
            int total_sessions = (++*params->total_sessions);
            int active_sessions = (++*params->active_sessions);
            pthread_mutex_unlock(params->snapshop_mutex);
            sem_post(params->chair_available);
            next_st->state = BEING_TUTORED;
            next_st->help = next_st->help + 1;
            next_st->tutor_id = thread_id;
            printf("T: Student %lu tutored by %lu. Students tutored now = %d. Total sessions tutored =%d\n", next_st->id, thread_id, active_sessions, total_sessions);
            usleep(200);                  // teach for 0.2ms;
            next_st->state = PROGRAMMING; // need a mutex ??
            pthread_mutex_lock(params->snapshop_mutex);
            active_sessions = (--*params->active_sessions);
            pthread_mutex_unlock(params->snapshop_mutex);
        }
    }
    return NULL;
}
void simulatecsms(int students, int tutors, int chairs, int helplimit)
{
    sem_t chair_available;
    sem_t chair_occupied;
    sem_t student_to_coordinator;
    sem_t coordinator_to_student;
    sem_init(&chair_occupied, 0, 0);
    sem_init(&chair_available, 0, chairs);
    sem_init(&student_to_coordinator,0,0);
    sem_init(&coordinator_to_student,0,0);

    int st_threads_left = students;
    pthread_mutex_t st_countex_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t snapshop_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t coordinator_access = PTHREAD_MUTEX_INITIALIZER;
    int total_requests = 0;
    int total_sessions = 0;
    int active_sessions = 0;
    int coordinator_thread_return = 0;

    struct student_wait_buffer *swb = initialize_wait_buffer(chairs);
    struct student* coordinator_input = NULL;

    struct student_thread_params st_params = {swb, &chair_available, &chair_occupied, &st_countex_mutex, &st_threads_left, &snapshop_mutex, &total_requests,&coordinator_access,&student_to_coordinator,&coordinator_to_student,&coordinator_input,&coordinator_thread_return,helplimit};
    struct tutor_thread_params tt_params = {swb, &chair_available, &chair_occupied, &st_countex_mutex, &st_threads_left, &snapshop_mutex, &total_sessions, &active_sessions};
    struct coordinator_thread_params ct_params = {swb,&coordinator_input,&snapshop_mutex,&total_requests,&chair_occupied,&student_to_coordinator,&coordinator_to_student,&coordinator_thread_return};
    
    pthread_t student_threads[students];
    pthread_t tutor_threads[tutors];
    pthread_t coordinator_thread_id;
    int i;
    for (i = 0; i < students; i++)
    {
        pthread_create(&student_threads[i], NULL, student_thread, (void *)&st_params);
    }
    pthread_create(&coordinator_thread_id,NULL,coordinator_thread,(void*)&ct_params);
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
    pthread_join(coordinator_thread_id,NULL);
    destroy_wait_buffer(swb);
    sem_destroy(&chair_occupied);
    sem_destroy(&chair_available);
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