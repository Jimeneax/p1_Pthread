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
void taskQueue(TaskQueue* queue){
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
        Task task = dequeueTask(queue);
        if (task.action == 'p'){
            update(task.number);
        } else if (task.action == 'w'){
            sleep(task.number);
        }
    }
}

int main(int argc, char* argv[])
{
    // check and parse command line options
    if (argc != 3) {
        printf("Usage: sum <infile>\n");
        exit(EXIT_FAILURE);
    }
    char *fn = argv[1];

    // open input file
    FILE* fin = fopen(fn, "r");
    if (!fin) {
        printf("ERROR: Could not open %s\n", fn);
        exit(EXIT_FAILURE);
    }

    // Verify that that number is greater than zero. 
    int numThread = atoi(argv[2]);
    if (numThread <= 0) {
        fprintf(stderr, "Number of threads must be greater than zero.\n");
        return EXIT_FAILURE;
    }

    TaskQueue taskQueue;
    initializeTaskQueue(&taskQueue);

    // Create worker threads
    pthread_t workerThreads[numThread];
    for (int i = 0; i < numThread; i++){
        pthread_create(&workerThreads[i], NULL, workerFunction, &taskQueue);
    }

    // load numbers and add them to the queue
    char action;
    long num;
    Task* newTask = (Task*)malloc(sizeof(Task));

    while (fscanf(fin, "%c %ld\n", &action, &num) == 2) {

        // check for invalid action parameters
        if (num < 1) {
            printf("ERROR: Invalid action parameter: %ld\n", num);
            exit(EXIT_FAILURE);
        }

        if (action == 'p') {            // process
            update(num);
        } else if (action == 'w') {     // wait
            sleep(num);
        } else {
            printf("ERROR: Unrecognized action: '%c'\n", action);
            exit(EXIT_FAILURE);
        }
    }
    
    fclose(fin);

    // Wait for all tasks to be processed
    for (int i = 0; i < numThread; i++){
        pthread_join(workerThreads[i], NULL);
    }

    // print results
    printf("%ld %ld %ld %ld\n", sum, odd, min, max);

    pthread_mutex_destroy(&taskQueue.mutex);
    pthread_cond_destroy(&taskQueue.cond);

    // clean up and return
    return (EXIT_SUCCESS);
}

