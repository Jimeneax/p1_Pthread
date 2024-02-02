/*
 * par_sum.c
 *
 * CS 470 Project 1 (Pthreads)
 * Serial version
 * 
 * Compile with --std=c99
 * Alberto Jimenez, Aaron Nyaanga
 * 
  • Did you use an AI-assist tool while constructing your solution? In what ways was it helpful (or not)? 
	    Yes, we use the git hub copilot to help us construct some of our helper methods. 
    We found that even when copilot predicted what we wanted to program we found some 
    mistakes in the code that we had to fix. So we had to be pay extra attention to what
    we were programming. 

  • How did you verify that your solution's performance scales linearly with the number of threads?
    Describe your experiments in detail. 
	    We verify that our solution performance scales linearly with threads when our 
    time decreases, and the threads increase. We tested the program with multiple small and large files
    and each time we doubled the threads the time when down in half.

  • How does your solution ensure the worker threads did not terminate until all tasks
    had entered the system? 
	    We use a done Boolean variable to ensure that our worker threads do not terminate
    before all the tasks are done.

  • How does your solution ensure that all of the worker threads are terminated cleanly
    when the supervisor is done? 
        We ensured that all worker threads are all terminated cleanly by using a Boolean to properly set to false in the beginning of the program. 
    Then we set it to true when the main thread has finished adding the tasks for the workers to finish.
    Then the pthread_cond_broadcast is used to tell all the worker threads waiting on taskQueue.cond that they should check the done flag. 
    Then In the workerFunction worker threads check the done flag while waiting for the tasks and if done == true then it exits.
    After setting done to true in the main thread we pthread_join to wait for all worker threads to finish.
    
	
  • Suppose that we wanted a priority-aware task queue. How would this affect your queue
    implementation, and how would it affect the threading synchronization? 
	    If we wanted a priority-aware task queue this would affect the enqueue and dequeue
    on sorting priority tasks so there would be a slowing initial time.

  • Suppose that we wanted task differentiation (e.g., some tasks can only be handled by
    some workers). How would this affect your solution? 
	    We would have to add a type of task variable that our worker threads would have to
    sort through to make sure they are doing the right type of tasks. This would affect
    the solution of the program time because sorting through the task would add time.

 */

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

// Task queue structure
typedef struct {
    char action;
    long number;
} Task;

// Node struct
typedef struct Node {
    Task task;
    struct Node* next;
} Node;

// Singly-linked list queue struct
typedef struct {
    Node* front;
    Node* rear;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} TaskQueue;
TaskQueue taskQueue;

volatile long sum = 0;
volatile long odd = 0;
volatile long min = INT_MAX;
volatile long max = INT_MIN;
volatile bool done = false;
pthread_mutex_t globalMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t globalCond = PTHREAD_COND_INITIALIZER;

// Function prototypes
void update(long number);
void initializeTaskQueue(TaskQueue* queue);
void enqueueTask(TaskQueue* queue, Task task);
Task dequeueTask(TaskQueue* queue);
void* workerFunction(void* arg);

/*
 * Update global aggregate variables given a number
 */
void update(long number)
{
    // simulate computation
    sleep(number);

    // Lock global mutex
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
    // Unlock global mutex
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

/*
 * Add task into the queue 
 */
void enqueueTask(TaskQueue* queue, Task task){
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->task = task;
    newNode->next = NULL;

    // Lock the queue mutex
    pthread_mutex_lock(&queue->mutex);

    if (queue->rear == NULL) {
        queue->front = newNode;
        queue->rear = newNode;
    } else {
        queue->rear->next = newNode;
        queue->rear = newNode;
    }

    pthread_cond_signal(&queue->cond);
    // Unlock the queue mutex
    pthread_mutex_unlock(&queue->mutex);
}

/*
 * Remove task from the queue
 */
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

/*
 * Worker thread function
 */
void* workerFunction(void* arg){
    TaskQueue* queue = (TaskQueue*)arg;
    while (1) {
        pthread_mutex_lock(&globalMutex);
        // Wait while task is not done
        while(taskQueue.front == NULL && !done){
            pthread_cond_wait(&taskQueue.cond, &globalMutex);
        }
        
        if(taskQueue.front == NULL && done){
            pthread_mutex_unlock(&globalMutex);
            pthread_exit(NULL);
        }
        // remove task when doen
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

/*
 * Main method
 */
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