#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

// globalni mutex
std::mutex mutex;
// globalni podminkova promenna
std::condition_variable condvar;

// pracovni cislo - nejaky kus sdilene pameti
int cislo;
// ktere vlakno je ted "na tahu"? 1 nebo 2
int turn = 1;

// prvni vlakno
void Thread_Fnc_1()
{
	while (true)
	{
		std::unique_lock<std::mutex> lck(mutex);

		// pokud nejsme na tahu, pockame
		while (turn != 1)
			condvar.wait(lck);

		std::cout << "=== VLAKNO 1" << std::endl;

		std::cout << "Pracuji... ";
		// predstirame nejakou praci
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		// potom udelame neco s cislem
		if (cislo % 12 == 0)
			cislo /= 2;
		else if (cislo % 3 == 0)
			cislo *= 12;
		else if (cislo < 10)
			cislo += 65;
		else
			cislo++;

		std::cout << "OK! cislo = " << cislo << std::endl;

		// prepneme "tah" na druhe vlakno a notifikujeme kohokoliv kdo ted spi nad touto podminkovou promennou
		turn = 2;
		condvar.notify_one();
	}
}

void Thread_Fnc_2()
{
	while (true)
	{
		std::unique_lock<std::mutex> lck(mutex);

		while (turn != 2)
			condvar.wait(lck);

		std::cout << "=== VLAKNO 2" << std::endl;

		std::cout << "Pracuji... ";
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		if (cislo > 10000)
			cislo /= 500;
		else if (cislo % 5 == 0)
			cislo /= 5;
		else if (cislo > 100)
			cislo /= 2;
		else
			cislo++;

		std::cout << "OK! cislo = " << cislo << std::endl;

		turn = 1;
		condvar.notify_one();
	}
}

int main(int argc, char** argv)
{
	cislo = 20;

	std::thread thr1(&Thread_Fnc_1);
	std::thread thr2(&Thread_Fnc_2);

	thr1.join();
	thr2.join();

	return 0;
}
