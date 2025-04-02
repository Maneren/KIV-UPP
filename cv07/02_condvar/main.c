#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/*
 * NOTE: v tomto souboru je nekde chyba, najdete ji a opravte ji
 */

// priznak - neni k dispozici vysledek
#define NO_RESULT -1
// priznak - ukonceni
#define RESULT_FLAG_EXIT -2

// sdileny mutex
pthread_mutex_t calc_mtx;
// sdilena podminkova promenna pro notifikaci
pthread_cond_t calc_cv;

// predavany vysledek vypoctu
int calc_result = NO_RESULT;

// vlaknova funkce generatoru - generuje cisla a notifikuje druhe vlakno k jejich zpracovani
// nasledne zacne pocitat dalsi vysledek a nez ho preda, tak pocka, az si druhe vlakno vysledek vyzvedne
// a notifikuje toto vlakno, ze uz muze predat dalsi vysledek
void *thread_1(void* par)
{
	printf("T1: start\n");

	int local_result;

	// generujeme 10 cisel
	for (int i = 0; i < 10; i++)
	{
		// simulujeme nejakou extremne slozitou praci, ktera zabere 50-500 ms
		usleep((50 + rand() % 500)*1000);
		local_result = rand() % 1024;

		// chceme predat vysledek - zamkneme mutex
		pthread_mutex_lock(&calc_mtx);

		// dokud predchozi vysledek neni zpracovany, cekame...
		while (calc_result != NO_RESULT)
			pthread_cond_wait(&calc_cv, &calc_mtx);

		// ulozime vysledek
		calc_result = local_result;

		printf("T1: post %d\n", calc_result);

		// notifikujeme druhe vlakno, aby mohlo zpracovat vysledek
		pthread_cond_signal(&calc_cv);
	}

	printf("T1: finishing, setting exit flag...\n");

	// vypocet je u konce, jeste musime signalizovat druhe vlakno, ze koncime

	// zamkneme mutex
	pthread_mutex_lock(&calc_mtx);

	// pockame, az druhe vlakno zpracuje posledni dil prace
	while (calc_result != NO_RESULT)
		pthread_cond_wait(&calc_cv, &calc_mtx);

	// nastavime priznak ukonceni
	calc_result = RESULT_FLAG_EXIT;

	// signalizujeme a odemkneme
	pthread_cond_signal(&calc_cv);
	pthread_mutex_unlock(&calc_mtx);

	// a ted uz se muzeme v klidu ukoncit
	printf("T1: exiting\n");

	return NULL;
}

// druhe vlakno - ceka na vysledek od prvniho vlakna, 'nejak' ho zpracuje a vyprazdni
// je ukonceno az kdyz prvni vlakno nastavi priznak konce
void *thread_2(void* par)
{
	printf("T2: start\n");

	int local_result;

	do
	{
		// zamkneme mutex - cekam na vysledek
		pthread_mutex_lock(&calc_mtx);

		// dokud tam nejaky neni, cekame na podmince
		while (calc_result == NO_RESULT)
			pthread_cond_wait(&calc_cv, &calc_mtx);

		// pokud jde o priznak ukonceni, ukoncime se
		if (calc_result == RESULT_FLAG_EXIT)
		{
			printf("T2: received exit flag\n");
			// nesmime zapomenout odemknout mutex!!!
			// tady to sice nema takovy smysl - uz nikdo zamykat nebude, ale zvykneme si na to
			pthread_mutex_unlock(&calc_mtx);
			break;
		}

		printf("T2: get %d\n", calc_result);
		
		// vyzvedneme si vysledek a sdilenou promennou nastavime na prazdno (aby ji mohlo prvni vlakno zase modifikovat)
		local_result = calc_result;
		calc_result = NO_RESULT;

		pthread_cond_signal(&calc_cv);
		pthread_mutex_unlock(&calc_mtx);

		// simulujeme nejakou dalsi praci s vysledkem, uz mimo kritickou sekci
		usleep((5 + rand()%10)*1000);
	}
	while (1);

	printf("T2: exiting\n");

	return NULL;
}

int main(int argc, char** argv)
{
	srand(time(NULL));

	printf("Start\n");

	pthread_t t1, t2;

	// inicializace mutexu a podminkove promenne
	pthread_mutex_init(&calc_mtx, NULL);
	pthread_cond_init(&calc_cv, NULL);

	// vytvorime vlakna
	pthread_create(&t1, NULL, thread_1, NULL);
	pthread_create(&t2, NULL, thread_2, NULL);

	// pockame na dokonceni
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	// znicime mutex a podminkovou promennou
	pthread_mutex_destroy(&calc_mtx);
	pthread_cond_destroy(&calc_cv);

	printf("Exit\n");

	return 0;
}
