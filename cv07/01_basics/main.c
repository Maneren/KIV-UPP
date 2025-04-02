#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// mutex pro pristup do souboru a inicializaci
pthread_mutex_t file_mtx;
// otevreny soubor; NULL = jeste nebyl otevreny
FILE* out = NULL;

// struktura nastaveni vlakna
typedef struct
{
	// identifikator vlakna
	int id;
	// nazev vlakna
	const char* name;
} thread_param_t;

// zapise do souboru retezec, otevre soubor, pokud je to nutne
void write_to_file(const char* str)
{
	// zamkneme mutex
	pthread_mutex_lock(&file_mtx);

	// otevre soubor, pokud neni
	if (out == NULL)
		out = fopen("output.txt", "w");

	// pokud se nepovedlo otevrit, koncime
	if (out == NULL)
		return; // !!! neco jsme ale zapomneli !!!

	fputs(str, out);

	// odemkneme mutex
	pthread_mutex_unlock(&file_mtx);
}

// vlakno generatoru cisel
void *generator_thread(void* par)
{
	// prebereme si parametry a pretypujeme je
	thread_param_t* param = (thread_param_t*)par;

	// vypiseme nejakou hlasku
	printf("[ID %d] jmeno %s\n", param->id, param->name);

	// 10-20 cisel
	long long pocet = 10 + rand() % 10;

	// nejaky uvodni delay
	usleep(10000);

	char buf[32];

	// vygenerujeme nekolik cisel a zapiseme je do souboru
	for (long long i = 0; i < pocet; i++)
	{
		sprintf(buf, "[ID %d] n = %d\n", param->id, rand() % 1000);
		write_to_file(buf);
		usleep((10 + rand() % 20) * 1000); // umele zpozdeni 10-30 ms
	}

	printf("[ID %d] konec\n", param->id);

	// a vratime pocet vygenerovanych cisel, natvrdo pretypovany na void*
	// pozor na zarovnani a podobne veci, vracene cislo by melo byt datoveho typu stejne
	// velkeho, jako je void* (tedy 8 bajtu na 64bit architekture, 4 bajty na 32bit)
	return (void*)pocet;
}	

int main(int argc, char** argv)
{
	srand(time(NULL));

	pthread_t t1, t2;
	// inicializace sdileneho mutexu
	pthread_mutex_init(&file_mtx, NULL);

	// parametry prvniho vlakna
	thread_param_t p1;
	p1.id = 1;
	p1.name = "Generator 1";
	// vytvorime vlakno (timto se okamzite spusti)
	pthread_create(&t1, NULL, generator_thread, &p1);

	// parametry druheho vlakna
	thread_param_t p2;
	p2.id = 2;
	p2.name = "Generator 2";
	// vytvorime vlakno
	pthread_create(&t2, NULL, generator_thread, &p2);

	long long num1, num2;

	// pockame na dokonceni vlakna 1 a 2 a vyzvedneme si navratove hodnoty
	pthread_join(t1, (void**)&num1);
	pthread_join(t2, (void**)&num2);

	if (out)
		fclose(out);

	printf("Vlakno 1 vygenerovalo %d hodnot\n", num1);
	printf("Vlakno 2 vygenerovalo %d hodnot\n", num2);

	return 0;
}
