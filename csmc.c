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
};

struct student_wait_buffer
{
    const int size;
    struct student **arr;
    int *is_open;
    int open_positions;
    pthread_mutex_t *lock;
};
struct student_wait_buffer *initialize_wait_buffer(int size)
{
    struct student_wait_buffer *buffer = (struct student_wait_buffer *)malloc(sizeof(struct student_wait_buffer));
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

void insert(struct student_wait_buffer *buffer, struct student *input_student)
{
    pthread_mutex_lock(buffer->lock);
    // find the open position
    if (buffer->open_positions > 0)
    {
        int i;
        for (i = 0; (i < buffer->size) && (buffer->is_open[i] == 0); i++)
            ;
        // check if it is open at i.
        if (i < buffer->size && buffer->is_open[i] == 1)
        {
            buffer->arr[i] = input_student;
            buffer->is_open[i] = 0;
            buffer->open_positions = buffer->open_positions - 1;
            printf("S: Student %lu takes a seat. Empty chairs = %d", input_student->id, buffer->open_positions);
        }
    }
    pthread_mutex_unlock(buffer->lock);
}

struct student *pop(struct student_wait_buffer *buffer)
{
    struct student *next_std_ptr = NULL;
    pthread_mutex_lock(buffer->lock);
    int i;
    int min_help = INT32_MAX;
    int pick = -1;
    for (i = 0; i < buffer->size; i++)
    {
        if (buffer->is_open[i] == 0 && min_help > buffer->arr[i]->help)
        {
            min_help = buffer->arr[i]->help;
            pick = i;
        }
    }
    if (pick != -1)
    {
        next_std_ptr = buffer->arr[pick];
        buffer->is_open[i] = 1;
        buffer->open_positions = buffer->open_positions + 1;
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
    // sem_wait(chair_available);
    while (st->help < params->help_max)
    {
        while (sem_trywait(params->chair_available) != 0)
        {
            // sleep for 2 ms
            usleep(2000);
        }
        // occupy chair and wait
        insert(params->buffer, st);
        sem_post(params->chair_occupied);
        while (st->being_tutored == 0)
        {
            usleep(200);
        }
        // once starting to tutor print the tutor ID;
        st->being_tutored = 0;
        st->tutor_id = -1;
    }
    return NULL;
}

struct coordinator_thread_params
{
    sem_t *chair_occupied;
    sem_t *chair_available;
    sem_t *waiting_for_tutor;
    sem_t *tutor_available;
};

void *coordinator_thread(void *args)
{
    struct coordinator_thread_params *params = (struct coordinator_thread_params *)args;

    while (1)
    {
        sem_wait(params->chair_occupied);    // wait for a student to occupy a waiting chair
        sem_wait(params->tutor_available);   // check if a tutor is available
        sem_post(params->waiting_for_tutor); // wake up tutor
        sem_post(params->chair_available);   // signal a chair is vacant(to all student threads)
    }
    return NULL;
}

struct tutor_thread_params
{
    struct student_wait_buffer *buffer;
    sem_t *waiting_for_tutor;
    sem_t *tutor_available;
};

void *tutor_thread(void *args)
{
    struct tutor_thread_params *params = (struct tutor_thread_params *)args;
    sem_wait(params->waiting_for_tutor);           // wait for wake up signal from coordinator_thread
    struct student *next_st = pop(params->buffer); // pick the next student based on priority from a synchronized variable
    // notify student thread about teaching
    next_st->tutor_id = (unsigned long)pthread_self();
    next_st->being_tutored = 1;
    next_st->help = next_st->help + 1;
    usleep(200); // teach for 0.2ms
    printf("T: Student %lu tutored by Tutor %lu. Students tutored now = %d. Total sessions tutored = %d", next_st->id, next_st->tutor_id, 0, 0);
    sem_post(params->tutor_available); // signal availability
    return NULL;
}

void simulatecsms(int students, int tutors, int chairs, int helplimit)
{
    printf("1");
    sem_t *chair_occupied = sem_open("chair_occupied", O_CREAT | O_EXCL, 0644, 0);
    sem_t *chair_available = sem_open("chair_available", O_CREAT | O_EXCL, 0644, chairs);
    ;
    sem_t *waiting_for_tutor = sem_open("waiting_for_tutor", O_CREAT | O_EXCL, 0644, 0);
    ;
    sem_t *tutor_available = sem_open("tutor_available", O_CREAT | O_EXCL, 0644, tutors);
    ;
    printf("2");
    struct student_wait_buffer *swb = initialize_wait_buffer(chairs);
    struct student_thread_params st_params = {swb, chair_occupied, chair_available, helplimit};
    struct coordinator_thread_params ct_params = {chair_occupied, chair_available, waiting_for_tutor, tutor_available};
    struct tutor_thread_params tt_params = {swb, waiting_for_tutor, tutor_available};
    pthread_t student_threads[students];
    pthread_t coordinator_thread_id;
    pthread_t tutor_threads[tutors];
    // create threads for cordinator, students, tutors
    printf("3");
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
    printf("4");
    // join threads
    for (i = 0; i < students; i++)
    {
        pthread_join(student_threads[i], NULL);
    }
    pthread_join(coordinator_thread_id, NULL);
    for (i = 0; i < tutors; i++)
    {
        pthread_join(tutor_threads[i], NULL);
    }
    printf("5");
    // close semaphores
    sem_close(chair_occupied);
    sem_close(chair_available);
    sem_close(waiting_for_tutor);
    sem_close(tutor_available);
    // unlink semaphores
    printf("6");
    sem_unlink("chair_occupied");
    sem_unlink("chair_available");
    sem_unlink("waiting_for_tutor");
    sem_unlink("tutor_available");
    printf("%d %d %d %d", students, tutors, chairs, helplimit);
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