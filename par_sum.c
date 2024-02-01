/*
 * sum.c
 *
 * CS 470 Project 1 (Pthreads)
 * Serial version
 * 
 * Compile with --std=c99
 * Alberto Jimenez, Aaron Nyaanga
 */

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

// aggregate variables
long sum = 0;
long odd = 0;
long min = INT_MAX;
long max = INT_MIN;
bool done = false;

// Task queue structure
typedef struct {
    char action;
    long number;
} Task;

typedef struct Node {
    Task task;
    struct Node* next;
} Node;

typedef struct {
    Node* front;
    Node* rear;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} TaskQueue;
TaskQueue taskQueue;
pthread_mutex_t globalMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t globalCond = PTHREAD_COND_INITIALIZER;

// function prototypes
void update(long number);
void initializeTaskQueue(TaskQueue* queue);
void enqueueTask(TaskQueue* queue, Task task);
Task dequeueTask(TaskQueue* queue);
void* workerFunction(void* arg);

/*
 * update global aggregate variables given a number
 */
void update(long number)
{
    // simulate computation
    sleep(number);

    // update aggregate variables
    pthread_mutex_lock(&globalMutex);
    sum += number;
    if (number % 2 == 1) {
        odd++;
    }
    if (number < min) {
        min = number;
    }
    if (number > max) {
        max = number;
    }
    pthread_mutex_unlock(&globalMutex);

}

/*
 * Initialize the task queue
 */
void initializeTaskQueue(TaskQueue* queue){
    queue->front = NULL;
    queue->rear = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void enqueueTask(TaskQueue* queue, Task task){
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->task = task;
    newNode->next = NULL;

    pthread_mutex_lock(&queue->mutex);

    if (queue->rear == NULL) {
        queue->front = newNode;
        queue->rear = newNode;
    } else {
        queue->rear->next = newNode;
        queue->rear = newNode;
    }

    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

Task dequeueTask(TaskQueue* queue) {
    
    pthread_mutex_lock(&queue->mutex);

    // Wait until there is a task in the queue
    while (queue->front == NULL) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }

    Task task = queue->front->task;
    Node* temp = queue->front;

    if (queue->front == queue->rear) {
        queue->front = NULL;
        queue->rear = NULL;
    } else {
        queue->front = queue->front->next;
    }

    free(temp);

    pthread_mutex_unlock(&queue->mutex);

    return task;
}

// Worker thread function
void* workerFunction(void* arg){
    TaskQueue* queue = (TaskQueue*)arg;
    while (1) {
        pthread_mutex_lock(&globalMutex);
        while(taskQueue.front == NULL && !done){
            pthread_cond_wait(&taskQueue.cond, &globalMutex);
        }
        if(taskQueue.front == NULL && done){
            pthread_mutex_unlock(&globalMutex);
            pthread_exit(NULL);
        }

        Task task = dequeueTask(queue);
        pthread_mutex_unlock(&globalMutex);

        if (task.action == 'p'){
            update(task.number);
        } else if (task.action == 'w'){
            sleep(task.number);
        }else{
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf("Usage: %s <infile> <num_threads>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char* fn = argv[1];
    int num_threads = atoi(argv[2]);
    if (num_threads <= 0) {
        fprintf(stderr, "Number of threads should be greater than 0.\n");
        exit(EXIT_FAILURE);
    }
    // open input file
    FILE* fin = fopen(fn, "r");
    if (!fin) {
        perror("ERROR: Could not open file");
        exit(EXIT_FAILURE);
    }
    // Initialize task queue
    initializeTaskQueue(&taskQueue);
    // Load numbers and add them to the queue
    char action;
    long num;
    while (fscanf(fin, "%c %ld\n", &action, &num) == 2) {
        // check for invalid action parameters
        if (num < 1) {
            fprintf(stderr, "ERROR: Invalid action parameter: %ld\n", num);
            exit(EXIT_FAILURE);
        }
        Task task = {action, num};
        // enqueue the task
        enqueueTask(&taskQueue, task);
    }

    fclose(fin);

  

    // Create worker threads
    pthread_t worker_threads[num_threads];
    for (int i = 0; i < num_threads; ++i) {
        if (pthread_create(&worker_threads[i], NULL, workerFunction, &taskQueue)) {
            perror("Error creating worker thread");
            exit(EXIT_FAILURE);
        }
    }
      // Signal workers that there are no more tasks
pthread_mutex_lock(&globalMutex);
done = true;
pthread_cond_broadcast(&taskQueue.cond);
pthread_mutex_unlock(&globalMutex);

    // Wait for worker threads to finish
    for (int i = 0; i < num_threads; ++i) {
        pthread_join(worker_threads[i], NULL);
    }

    // print results
    printf("%ld %ld %ld %ld\n", sum, odd, min, max);

    // clean up and return
    return EXIT_SUCCESS;
}

