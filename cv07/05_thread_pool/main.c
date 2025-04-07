#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define bool unsigned char
#define true 1
#define false 0

void task_1(int task_id) {
  usleep((500 + rand() % 1000) * 1000);
  printf("Uloha %i typu 1 hotova!\n", task_id);
}

void task_2(int task_id) {
  usleep((800 + rand() % 800) * 1000);
  printf("Uloha %i typu 2 hotova!\n", task_id);
}

void task_3(int task_id) {
  usleep((1000 + rand() % 600) * 1000);
  printf("Uloha %i typu 3 hotova!\n", task_id);
}

typedef void (*taskFnc_t)(int);

// UKOL: vytvorte 8 vlaken, ktere budou zpracovavat ulohy typu 1, 2 a 3
//       - ulohy mohou prijit kdykoliv, tzn. neni dobre vytvaret pokazde nove
//       specializovane vlakno
//           - hlavni vlakno bude vytvaret ulohy a predavat je vlaknum funkci
//           submit_task
//       - udrzujte si pole 8 vlaken, ktere kazde bude cekat na prichod ulohy
//       - vlakno bude z fronty prebirat ulohy formou ukazatele na funkci
//       (taskFnc_t) a jeji poradove cislo

// thread pool
#define THREAD_POOL_SIZE 8
pthread_t thread_pool[THREAD_POOL_SIZE];

// queue of tasks
#define QUEUE_SIZE 100
taskFnc_t task_queue[QUEUE_SIZE];
int id_queue[QUEUE_SIZE];
int queue_head = 0;
int queue_tail = 0;

pthread_mutex_t mutex;
pthread_cond_t cond;

atomic_int active_tasks = 0;
atomic_bool active_flag = true;

void *thread_func(void *arg) {
  // sorry, I know it's ugly
  size_t thread_id = (size_t)arg;

  for (;;) {
    pthread_mutex_lock(&mutex);

    bool is_active = true;

    while ((is_active =
                atomic_load_explicit(&active_flag, memory_order_acquire)) &&
           (queue_head == queue_tail))
      pthread_cond_wait(&cond, &mutex);

    if (!is_active) {
      pthread_mutex_unlock(&mutex);

      printf("Thread %lu finished\n", thread_id);
      return NULL;
    }

    taskFnc_t task = task_queue[queue_head];
    int task_id = id_queue[queue_head];
    queue_head = (queue_head + 1) % QUEUE_SIZE;
    pthread_mutex_unlock(&mutex);

    printf("Running task %d on thread %lu\n", task_id, thread_id);
    task(task_id);

    atomic_fetch_sub_explicit(&active_tasks, 1, memory_order_release);
  }
}

void submit_task(int task_id, taskFnc_t func) {
  pthread_mutex_lock(&mutex);
  printf("Spawning task %i\n", task_id);
  task_queue[queue_tail] = func;
  id_queue[queue_tail] = task_id;
  queue_tail = (queue_tail + 1) % QUEUE_SIZE;
  atomic_fetch_add_explicit(&active_tasks, 1, memory_order_release);
  pthread_mutex_unlock(&mutex);
  pthread_cond_signal(&cond);
}

int main(int argc, char **argv) {
  srand(time(NULL));

  for (size_t i = 0; i < THREAD_POOL_SIZE; i++) {
    pthread_create(&thread_pool[i], NULL, thread_func, (void *)i);
  }

  // priklad uloh ke zpracovani
  for (size_t i = 0; i < 20; i++) {
    size_t task_type = rand() % 3;

    switch (task_type) {
    case 0:
      submit_task(i, &task_1);
      break;
    case 1:
      submit_task(i, &task_2);
      break;
    case 2:
      submit_task(i, &task_3);
      break;
    }
  }

  while (atomic_load_explicit(&active_tasks, memory_order_acquire)) {
    usleep(100);
  }

  printf("All tasks done! Initiating shutdown...\n");

  atomic_store_explicit(&active_flag, false, memory_order_release);

  for (size_t i = 0; i < THREAD_POOL_SIZE; i++) {
    pthread_cond_signal(&cond);
  }

  for (size_t i = 0; i < THREAD_POOL_SIZE; i++) {
    pthread_join(thread_pool[i], NULL);
  }

  printf("Program dokoncil praci!\n");

  return 0;
}
