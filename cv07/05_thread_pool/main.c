#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

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

typedef void(*taskFnc_t)(int);

// UKOL: vytvorte 8 vlaken, ktere budou zpracovavat ulohy typu 1, 2 a 3
//       - ulohy mohou prijit kdykoliv, tzn. neni dobre vytvaret pokazde nove specializovane vlakno
//           - hlavni vlakno bude vytvaret ulohy a predavat je vlaknum funkci submit_task
//       - udrzujte si pole 8 vlaken, ktere kazde bude cekat na prichod ulohy
//       - vlakno bude z fronty prebirat ulohy formou ukazatele na funkci (taskFnc_t) a jeji poradove cislo

void thread_func() {
	// TODO: dokud je ve fronte nejaka uloha a hlavni vlakno nesignalizovalo konec, prebirat ulohy a zpracovavat je
}

void submit_task(int task_id, taskFnc_t func) {
	// TODO: ulozit ulohu do fronty a signalizovat vlaknum, ze je tam nova uloha
}

int main(int argc, char** argv) {
	srand(time(NULL));

	// TODO: inicializace, vytvoreni a spusteni 8 vlaken

	// priklad uloh ke zpracovani
	for (int i = 0; i < 20; i++) {
		int task_type = rand() % 3;

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

	// TODO: signalizace vlaknum, ze uz nebudou zadne ulohy, korektni ukonceni vlaken a programu

	printf("Program dokoncil praci!\n");

	return 0;
}
