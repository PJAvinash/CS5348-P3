#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

struct student
{
    unsigned long id;
    unsigned long tutor_id;
    int help;
    int being_tutored;
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

// returns 1 when successful else 0
int insert(struct student_wait_buffer *buffer, struct student *input_student)
{
    int is_success = 0;
    pthread_mutex_lock(buffer->lock);
    // find the open position
    printf("%lu: open_positions:%d\n", (unsigned long)pthread_self(), buffer->open_positions);
    if (buffer->open_positions > 0)
    {
        int i;
        // linear search
        for (i = 0; (i < buffer->size) && (buffer->is_open[i] == 0); i++)
        {
        }
        // check if it is open at i.
        if (i < buffer->size && buffer->is_open[i] == 1)
        {
            buffer->arr[i] = input_student;
            buffer->is_open[i] = 0;
            buffer->open_positions = buffer->open_positions - 1;
            is_success = 1;
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
    }else{
        printf("pop open_positions:%d\n",buffer->open_positions);
    }
    pthread_mutex_unlock(buffer->lock);
    return next_std_ptr;
}

struct student_thread_params
{
    struct student_wait_buffer *buffer;
    sem_t *chair_occupied;
    sem_t *chair_available;
    int help_max;
    pthread_mutex_t *enque;
    pthread_mutex_t *counter_mutex;
    int *st_threads_left;
};

void *student_thread(void *args)
{
    struct student_thread_params *params = (struct student_thread_params *)args;
    pthread_t thread_id = pthread_self();
    struct student *st = (struct student *)malloc(sizeof(struct student));
    st->id = (unsigned long)thread_id;
    st->help = 0;
    st->being_tutored = 0;
    st->tutor_id = -1;
    pthread_mutex_init(&st->mutex, NULL);
    while (1)
    {
        pthread_mutex_lock(&st->mutex);
        if (st->help == params->help_max)
        {
            pthread_mutex_unlock(&st->mutex);
            break;
        }
        pthread_mutex_unlock(&st->mutex);
        sem_wait(params->chair_available);
        pthread_mutex_lock(params->enque);
        int insertstatus = insert(params->buffer, st);
        pthread_mutex_unlock(params->enque);
        if (insertstatus)
        {
            sem_post(params->chair_occupied);
            printf("S: Student %lu takes a seat. Empty chairs = %d\n", st->id, params->buffer->open_positions);
            while (st->being_tutored == 0)
            {
                //wait in lobby to get picked
            }
            while (st->being_tutored == 1)
            {
                //wait for tutoring being completed
            }
            printf("S: Student %lu received help from Tutor %lu.\n", st->id, st->tutor_id);
            usleep(2000); // go to programming
                          // once starting to tutor print the tutor ID;
            pthread_mutex_lock(&st->mutex);
            st->being_tutored = 0;
            st->tutor_id = -1;
            pthread_mutex_unlock(&st->mutex);
        }
        else
        {
            printf("S: Student %lu insert failed\n", st->id);
        }
    }
    pthread_mutex_lock(params->counter_mutex);
    (*params->st_threads_left)--;
    printf("S: Student %lu completed help:%d. students left:%d\n", st->id, st->help, *params->st_threads_left);
    pthread_mutex_unlock(params->counter_mutex);
    return NULL;
}

struct coordinator_thread_params
{
    sem_t *chair_occupied;
    sem_t *chair_available;
    sem_t *waiting_for_tutor;
    sem_t *tutor_available;
    sem_t *tutor_accepted;
    pthread_mutex_t *counter_mutex;
    int *st_threads_left;
};

void *coordinator_thread(void *args)
{
    struct coordinator_thread_params *params = (struct coordinator_thread_params *)args;
    while (1)
    {
        pthread_mutex_lock(params->counter_mutex);
        if (*params->st_threads_left == 0)
        {
            pthread_mutex_unlock(params->counter_mutex);
            break;
        }
        pthread_mutex_unlock(params->counter_mutex);
        sem_wait(params->chair_occupied);    // wait for a student to occupy a waiting chair
        sem_wait(params->tutor_available);   // check if a tutor is available
        // inform the student about tutor availabity ?

        sem_post(params->waiting_for_tutor); // wake up tutor
        sem_wait(params->tutor_accepted);    // tutor completes the pop operation
        sem_post(params->chair_available);   // signal a chair is vacant(to all student threads)
    }
    return NULL;
}

struct tutor_thread_params
{
    struct student_wait_buffer *buffer;
    sem_t *waiting_for_tutor;
    sem_t *tutor_available;
    sem_t *tutor_accepted;
    pthread_mutex_t *deque;
    pthread_mutex_t *counter_mutex;
    int *st_threads_left;
};

void *tutor_thread(void *args)
{

    printf("tt-1\n");
    struct tutor_thread_params *params = (struct tutor_thread_params *)args;
    while (1)
    {
        pthread_mutex_lock(params->counter_mutex);
        if (*params->st_threads_left == 0)
        {
            pthread_mutex_unlock(params->counter_mutex);
            break;
        }
        pthread_mutex_unlock(params->counter_mutex);
        sem_wait(params->waiting_for_tutor); // wait for wake up signal from coordinator_thread
        pthread_mutex_lock(params->deque);
        struct student *next_st = pop(params->buffer); // pick the next student based on priority from a synchronized variable
        pthread_mutex_unlock(params->deque);
        if (next_st != NULL)
        {
            sem_post(params->tutor_accepted);
            // notify student thread about teaching
            pthread_mutex_lock(&next_st->mutex);
            next_st->tutor_id = (unsigned long)pthread_self();
            next_st->help = next_st->help + 1; // not safe
            next_st->being_tutored = 1;
            pthread_mutex_unlock(&next_st->mutex);
            usleep(200);
            // printf("tt-3\n");
            // usleep(200); // teach for 0.2ms
            // printf("T: Student %lu tutored by Tutor %lu. Students tutored now = %d. Total sessions tutored = %d", next_st->id, next_st->tutor_id, 0, 0);
            sem_post(params->tutor_available); // signal availability
        }else{
            printf("T: popping stuent failed \n");
        }
    }
    return NULL;
}

void simulatecsms(int students, int tutors, int chairs, int helplimit)
{
    printf("1\n");
    sem_t chair_occupied;
    sem_t chair_available;
    sem_t waiting_for_tutor;
    sem_t 
    sem_t *chair_occupied = sem_open("chair_occupied", O_CREAT | O_EXCL, 0644, 0);
    sem_t *chair_available = sem_open("chair_available", O_CREAT | O_EXCL, 0644, chairs);
    sem_t *waiting_for_tutor = sem_open("waiting_for_tutor", O_CREAT | O_EXCL, 0644, 0);
    sem_t *tutor_available = sem_open("tutor_available", O_CREAT | O_EXCL, 0644, tutors);
    sem_t *tutor_accepted = sem_open("tutor_accepted", O_CREAT | O_EXCL, 0644, 0);
    printf("2\n");
    pthread_mutex_t st_countex_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t enque = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t deque = PTHREAD_MUTEX_INITIALIZER;
    int st_threads_left = students;
    struct student_wait_buffer *swb = initialize_wait_buffer(chairs);
    struct student_thread_params st_params = {swb, chair_occupied, chair_available, helplimit, &enque, &st_countex_mutex, &st_threads_left};
    struct coordinator_thread_params ct_params = {chair_occupied, chair_available, waiting_for_tutor, tutor_available, tutor_accepted, &st_countex_mutex, &st_threads_left};
    struct tutor_thread_params tt_params = {swb, waiting_for_tutor, tutor_available, tutor_accepted, &deque, &st_countex_mutex, &st_threads_left};
    pthread_t student_threads[students];
    pthread_t coordinator_thread_id;
    pthread_t tutor_threads[tutors];
    // create threads for cordinator, students, tutors
    printf("3\n");
    pthread_create(&coordinator_thread_id, NULL, coordinator_thread, (void *)&ct_params);
    int i;
    for (i = 0; i < students; i++)
    {
        pthread_create(&student_threads[i], NULL, student_thread, (void *)&st_params);
    }
    for (i = 0; i < tutors; i++)
    {
        pthread_create(&tutor_threads[i], NULL, tutor_thread, (void *)&tt_params);
    }
    printf("4\n");
    // join threads
    pthread_join(coordinator_thread_id, NULL);
    for (i = 0; i < tutors; i++)
    {
        pthread_join(tutor_threads[i], NULL);
    }
    printf("5\n");
    // close semaphores
    sem_close(chair_occupied);
    sem_close(chair_available);
    sem_close(waiting_for_tutor);
    sem_close(tutor_available);
    sem_close(tutor_accepted);
    // unlink semaphores
    printf("6\n");
    sem_unlink("chair_occupied");
    sem_unlink("chair_available");
    sem_unlink("waiting_for_tutor");
    sem_unlink("tutor_available");
    sem_unlink("tutor_accepted");
    printf("params: %d %d %d %d", students, tutors, chairs, helplimit);
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
