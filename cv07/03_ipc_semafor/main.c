#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

sem_t* ipc_sem;

void sighandler(int signum) {
	if (signum == SIGINT) {
		printf("SIGINT - zavirame semafor\n");

		sem_close(ipc_sem);
		sem_unlink("test_sem");

		exit(0);
	}
}

int main(int argc, char** argv) {

	signal(SIGINT, sighandler);

	// ziskame si PID procesu, abychom je ve vypisu mohli identifikovat
	int pid = getpid();

	// otevreme semafor 'test_sem'
	// vytvori se v /dev/sem/, /dev/shm/ a podobnych, podle systemu
	// (pozn. shm je zkratka pro "shared memory")
	ipc_sem = sem_open("test_sem", O_CREAT, 0664, 1);

	// udelame nejakych 10 dummy iteraci
	for (int i = 0; i < 10; i++) {
		// zabereme semafor
		sem_wait(ipc_sem);

		printf("[%d] Iteration %d\n", pid, i);

		for (int j = 0; j < 10; j++) {
			printf("[%d] Working %d...\n", pid, j);
			usleep(100000);
		}

		printf("[%d] Done %d\n", pid, i);

		// vratime semafor
		sem_post(ipc_sem);

		usleep(100000);
	}

	// zavreme semafor
	sem_close(ipc_sem);
	// a nakonec ho smazeme - resp. posledni, co vola unlink, ho smaze
	sem_unlink("test_sem");

	return 0;
}
