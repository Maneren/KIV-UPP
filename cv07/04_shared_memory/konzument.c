#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int shm_fd;
void *ptr;

const char *mem_name = "/upp_prodcons";
const int mem_size = 4096;

void signal_handler(int signum) {
  if (signum == SIGINT) {
    printf("Prijaty SIGINT - ukoncuji\n");
    munmap(ptr, mem_size);
    close(shm_fd);
    exit(0);
  }
}

sem_t *prod_sem;
sem_t *cons_sem;

int main() {
  signal(SIGINT, signal_handler);

  shm_fd = shm_open(mem_name, O_RDONLY, 0666);
  if (shm_fd == -1) {
    printf("Nelze vytvorit sdilenou pamet\n");
    return 1;
  }

  ptr = mmap(0, mem_size, PROT_READ, MAP_SHARED, shm_fd, 0);
  if (ptr == MAP_FAILED) {
    printf("Nelze namapovat sdilenou pamet\n");
    return 1;
  }

  // TODO: vytvorte semafory pro synchronizaci
  prod_sem = sem_open("upp_prod", O_CREAT, 0664, 0);
  cons_sem = sem_open("upp_cons", O_CREAT, 0664, 0);

  sem_post(cons_sem);

  while (1) {
    // TODO: misto sleepu pockejte na semafor od producenta
    // usleep(1000000);
    sem_wait(prod_sem);

    printf("Prijato: %s\n", (char *)(ptr));

    // TODO: notifikujte semafor producenta
    sem_post(cons_sem);
  }

  sem_close(prod_sem);
  sem_close(cons_sem);
  sem_unlink("upp_prod");
  sem_unlink("upp_cons");

  munmap(ptr, mem_size);
  close(shm_fd);

  return 0;
}
