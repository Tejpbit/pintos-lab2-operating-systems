/* Tests categorical mutual exclusion with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "lib/random.h" //generate random numbers

#define BUS_CAPACITY 3
#define SENDER 0
#define RECEIVER 1
#define NORMAL 0
#define HIGH 1

/*
 *	initialize task with direction and priority
 *	call o
 * */
typedef struct {
	int direction;
	int priority;
} task_t;

void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive);

void senderTask(void *);
void receiverTask(void *);
void senderPriorityTask(void *);
void receiverPriorityTask(void *);


void oneTask(task_t task);/*Task requires to use the bus and executes methods below*/
void getSlot(task_t task); /* task tries to use slot on the bus */
void transferData(task_t task); /* task processes data on the bus either sending or receiving based on the direction*/
void leaveSlot(task_t task); /* task release the slot */

static struct semaphore bus;
static struct semaphore waiting_prio[2];
static struct semaphore waiting[2];
static struct lock lock;
static int direction = 0;

/* initializes semaphores */
void init_bus(void){
    random_init((unsigned int)123456789);

    sema_init(&bus, BUS_CAPACITY);

    sema_init(&waiting_prio[SENDER]);
    sema_init(&waiting_prio[RECEIVER]);
    sema_init(&waiting[SENDER]);
    sema_init(&waiting[RECEIVER]);

    lock_init(&lock);
}

/*
 *  Creates a memory bus sub-system  with num_tasks_send + num_priority_send
 *  sending data to the accelerator and num_task_receive + num_priority_receive tasks
 *  reading data/results from the accelerator.
 *
 *  Every task is represented by its own thread.
 *  Task requires and gets slot on bus system (1)
 *  process data and the bus (2)
 *  Leave the bus (3).
 */

void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive)
{
    int i;
    for (i = 0; i < 50; ++i)
    {
        thread_create("Sender", 0, senderTask, 0);
        //thread_create("Receiver", 0, receiverTask, 0);
    }
}

/* Normal task,  sending data to the accelerator */
void senderTask(void *aux UNUSED){
    task_t task = {SENDER, NORMAL};
    oneTask(task);
}

/* High priority task, sending data to the accelerator */
void senderPriorityTask(void *aux UNUSED){
    task_t task = {SENDER, HIGH};
    oneTask(task);
}

/* Normal task, reading data from the accelerator */
void receiverTask(void *aux UNUSED){
    task_t task = {RECEIVER, NORMAL};
    oneTask(task);
}

/* High priority task, reading data from the accelerator */
void receiverPriorityTask(void *aux UNUSED){
    task_t task = {RECEIVER, HIGH};
    oneTask(task);
}

/* abstract task execution*/
void oneTask(task_t task) {
  getSlot(task);
  transferData(task);
  leaveSlot(task);
}


/* task tries to get slot on the bus subsystem */
void getSlot(task_t task)
{
    if (bus->value == BUS_CAPACITY) {
        direction = task->direction;
    }

    cond_wait(direction, lock);

    if (task->priority == HIGH) {
        if (bus->value == 0 || task->direction != direction) {
            sema_down(&waiting_prio[task->direction]);
        }
    } else {
        if (bus->value == 0 || task->direction != direction) {
            sema_down(&waiting[task->direction]);
        }
    }
}

/* task processes data on the bus send/receive */
void transferData(task_t task)
{
    printf("Thread transfer data\n");
    timer_msleep (random_ulong() % 3000);
    printf("Thread transfer data done\n");
}

/* task releases the slot */
void leaveSlot(task_t task)
{
    // lock_acquire(&lock);

    sema_up(&bus);

    lock_acquire(&lock);
    if (list_size(waiting_prio[1-task->direction]->waiters) > 0
            && list_size(waiting_prio[task->direction]->waiters) == 0) {
        direction = 1 - direction;
    }
    lock_release(&lock);

    if (list_size(waiting_prio[task->direction]->waiters) > 0) {
        sema_up(&waiting_prio[task->direction]);
    } else if (list_size(waiting_prio[1-task->direction]->waiters) > 0) {
        sema_up(&waiting_prio[1-task->direction]);
    } else if (list_size(waiting[task->direction]->waiters) > 0) {
        sema_up(&waiting[task->direction]);
    } else if (list_size(waiting[1-task->direction]->waiters) > 0) {
        sema_up(&waiting[1-task->direction]);
    }

    // lock_release(&lock);
}
