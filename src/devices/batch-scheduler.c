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
static int batchNum;

static int started = 0, ended = 0, transfers = 0;

/* initializes semaphores */
void init_bus(void){
    random_init((unsigned int)123456789);

    sema_init(&bus, BUS_CAPACITY);

    sema_init(&waiting_prio[SENDER], 0);
    sema_init(&waiting_prio[RECEIVER], 0);
    sema_init(&waiting[SENDER], 0);
    sema_init(&waiting[RECEIVER], 0);

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

    for (i = 1; i <= num_priority_send; ++i)
    {
        char name[9];
        snprintf(name, 9, "SP%03d-%02d", batchNum, i);
        thread_create(name, 0, senderPriorityTask, 0);
    }

    for (i = 1; i <= num_priority_receive; ++i)
    {
        char name[9];
        snprintf(name, 9, "RP%03d-%02d", batchNum, i);
        thread_create(name, 0, receiverPriorityTask, 0);
    }

    for (i = 1; i <= num_tasks_send; ++i)
    {
        char name[9];
        snprintf(name, 9, "SN%03d-%02d", batchNum, i);
        thread_create(name, 0, senderTask, 0);
    }

    for (i = 1; i <= num_task_receive; ++i)
    {
        char name[9];
        snprintf(name, 9, "RN%03d-%02d", batchNum, i);
        thread_create(name, 0, receiverTask, 0);
    }

    batchNum++;
}

char directionChar(int direction) {
    if (direction)
    {
        return 'R';
    }
    return 'S';
}

void printInfo() {
    printf("%s    SN: %03d    SP: %03d    RN: %03d    RP: %03d    BUS: %d    Direction: %c    Started: %03d    Transfers: %03d    Ended: %03d\n",
        thread_current ()->name,
        list_size(&waiting[SENDER].waiters),
        list_size(&waiting_prio[SENDER].waiters),
        list_size(&waiting[RECEIVER].waiters),
        list_size(&waiting_prio[RECEIVER].waiters),
        BUS_CAPACITY - bus.value,
        directionChar(direction),
        started,
        transfers,
        ended);
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
    started++;
    bool bus_available = false, my_dir = false;
    while(!bus_available) {

        lock_acquire(&lock);
        if (bus.value == BUS_CAPACITY) { // Edge case: If i'm first, dictate the direction
            direction = task.direction;
        }

        my_dir = task.direction == direction;

        if (my_dir && (task.priority == HIGH || (task.priority == NORMAL && list_size(&(waiting_prio[task.direction].waiters)) == 0))) {
            bus_available = sema_try_down(&bus);
        }

        lock_release(&lock);

        if (!bus_available) {
            if (task.priority == HIGH) {
                sema_down(&waiting_prio[task.direction]);
            } else {
                sema_down(&waiting[task.direction]);
            }
        }

    }
    printInfo();
}

/* task processes data on the bus send/receive */
void transferData(task_t task)
{
    //printf("Begin (%s)\n", thread_current ()->name);
    timer_sleep ((int64_t)random_ulong() % 100);
    // timer_sleep ((int64_t)1000);
    //printf("Done (%s)\n", thread_current ()->name);

    transfers++;
}

int min (int a, int b) {
    if (a < b)
    {
        return a;
    }
    return b;
}

void wakeWaiters(int dir) {

    int i;
    int queue_size;
    int loop_num = bus.value;
    char* threadName = thread_current()->name;

    // Prio my direction
    queue_size = list_size(&(waiting_prio[dir].waiters));
    if (queue_size > 0) {

        loop_num = min(loop_num, queue_size);
        for (i = 0; i < loop_num; i++) {
            sema_up(&waiting_prio[dir]);
        }

        return;
    }

    // Prio other direction
    queue_size = list_size(&(waiting_prio[1-dir].waiters));
    if (queue_size > 0)
    {
        if (bus.value == BUS_CAPACITY) {

            // direction = 1-dir;

            loop_num = min(loop_num, queue_size);
            for (i = 0; i < loop_num; i++) {
                sema_up(&waiting_prio[1-dir]);
            }

        }
        return;
    }


    // My direction
    queue_size = list_size(&(waiting[dir].waiters));
    if (queue_size > 0) {
        loop_num = min(loop_num, queue_size);

        for (i = 0; i < loop_num; i++) {
            sema_up(&waiting[dir]);
        }

        return;
    }

    // Other direction
    queue_size = list_size(&(waiting[1-dir].waiters));
    if (queue_size > 0) {
        if (bus.value == BUS_CAPACITY) {

            // direction = 1-dir;

            loop_num = min(loop_num, queue_size);

            for (i = 0; i < loop_num; i++) {
                sema_up(&waiting[1-dir]);
            }

            return;
        }
    }

}

/* task releases the slot */
void leaveSlot(task_t task)
{

    lock_acquire(&lock);
    sema_up(&bus);

    wakeWaiters(task.direction);
    ended++;

    lock_release(&lock);
}