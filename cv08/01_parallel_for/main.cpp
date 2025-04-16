#include <iostream>
#include <random>
#include <vector>
#include <thread>
#include <memory>
#include <chrono>

// pocet cisel, se kterymi se bude pracovat
constexpr size_t NumbersCount = 100'000;
// kolikrat se ma provest experiment (a mereni)
constexpr size_t RunCount = 5;

// vektor cisel
std::vector<unsigned int> Numbers;
// kopie vektoru cisel - abychom meli stale stejne vychozi pole
std::vector<unsigned int> NumbersCopy;

// funkce pro mereni - ponechme bez vetsiho komentare, je stejna jako u predchozich cviceni
// jen jednou spusti na prazdno, a pak spusti nekolikrat mereni po sobe a zmeri prumerny cas
void Do_Measure(const std::string& name, void(*fnc)())
{
	std::cout << "Measurement: " << name << std::endl;

	Numbers = NumbersCopy;

	std::cout << "Dry run ...";
	fnc();
	std::cout << " OK" << std::endl;

	unsigned long long tm = 0;

	for (size_t i = 0; i < RunCount; i++)
	{
		std::cout << "Run " << i << " ... ";

		Numbers = NumbersCopy;

		auto st = std::chrono::steady_clock::now();

		fnc();

		auto end = std::chrono::steady_clock::now();
		tm += std::chrono::duration_cast<std::chrono::milliseconds>(end - st).count();

		std::cout << "OK" << std::endl;
	}
	tm /= RunCount;

	std::cout << "Average time: " << tm << "ms" << std::endl << std::endl;
}

/*
 * Nasleduje nekolik verzi algoritmu, ktery vynasobi kazde cislo poctem jeho vyskytu
 * Jde tedy o O(n^2) algoritmus, ktery musi fungovat na dve faze:
 *  1) pocitani vyskytu kazdeho cisla
 *  2) vynasobeni cisel
 * dve faze jsou nutne proto, ze jakmile vynasobime cislo ve vektoru, muzeme vyrobit vyskyt cisla, ktere tam predtim nebylo
 */

// seriova verze
void test_serial()
{
	std::vector<double> multiples(Numbers.size());

	// faze 1 - napocitani hodnot
	for (size_t i = 0; i < Numbers.size(); i++)
	{
		multiples[i] = 0;
		for (size_t j = 0; j < Numbers.size(); j++)
		{
			if (Numbers[i] == Numbers[j])
				multiples[i] += 1.0;
		}
	}

	// faze 2 - vynasobeni
	for (size_t i = 0; i < Numbers.size(); i++)
		Numbers[i] *= static_cast<unsigned int>(multiples[i]);
}

// paralelni verze - klasicka vlakna
void test_parallel_thread()
{
	std::vector<std::unique_ptr<std::thread>> workers;
	const size_t cpus = std::thread::hardware_concurrency();

	const size_t portion = Numbers.size() / cpus;

	std::vector<double> multiples(Numbers.size());

	// vytvoreni workeru pro fazi 1
	for (size_t w = 0; w < cpus; w++)
	{
		workers.push_back(std::make_unique<std::thread>([&](size_t begin, size_t end) {

			for (size_t i = begin; i < end; i++)
			{
				multiples[i] = 0;
				for (size_t j = 0; j < Numbers.size(); j++)
				{
					if (Numbers[i] == Numbers[j])
						multiples[i] += 1.0;
				}
			}

		}, w*portion, (w+1)*portion));
	}

	// pockani na workery
	for (size_t w = 0; w < cpus; w++)
	{
		if (workers[w]->joinable())
			workers[w]->join();
	}

	workers.clear();

	// vytvoreni workeru pro fazi 2
	for (size_t w = 0; w < cpus; w++)
	{
		workers.push_back(std::make_unique<std::thread>([&](size_t begin, size_t end) {

			for (size_t i = begin; i < end; i++)
				Numbers[i] *= static_cast<unsigned int>(multiples[i]);

		}, w*portion, (w+1)*portion));
	}

	// pockani na workery
	for (size_t w = 0; w < cpus; w++)
	{
		if (workers[w]->joinable())
			workers[w]->join();
	}
}

// paralelni verze - OpenMP a staticka delba prace
void test_openmp_static()
{
	// jak lze videt, jde v podstate jen o ten samy kod, jako v pripade seriove verze
	// jen je navic dekorovany direktivami prekladace pro paralelizaci s OpenMP

	std::vector<double> multiples(Numbers.size());

#pragma omp parallel for schedule(static)
	for (int i = 0; i < Numbers.size(); i++)
	{
		multiples[i] = 0;
		for (size_t j = 0; j < Numbers.size(); j++)
		{
			if (Numbers[i] == Numbers[j])
				multiples[i] += 1.0;
		}
	}

#pragma omp parallel for schedule(static)
	for (int i = 0; i < Numbers.size(); i++)
		Numbers[i] *= static_cast<unsigned int>(multiples[i]);
}

// paralelni verze - OpenMP a dynamicka delba prace
void test_openmp_dynamic()
{
	std::vector<double> multiples(Numbers.size());

#pragma omp parallel for schedule(dynamic)
	for (int i = 0; i < Numbers.size(); i++)
	{
		multiples[i] = 0;
		for (size_t j = 0; j < Numbers.size(); j++)
		{
			if (Numbers[i] == Numbers[j])
				multiples[i] += 1.0;
		}
	}

#pragma omp parallel for schedule(dynamic)
	for (int i = 0; i < Numbers.size(); i++)
		Numbers[i] *= static_cast<unsigned int>(multiples[i]);
}

// paralelni verze - OpenMP a dynamicka delba prace s granularitou
void test_openmp_dynamic_gran()
{
	std::vector<double> multiples(Numbers.size());

#pragma omp parallel for schedule(dynamic, 2)
	for (int i = 0; i < Numbers.size(); i++)
	{
		multiples[i] = 0;
		for (size_t j = 0; j < Numbers.size(); j++)
		{
			if (Numbers[i] == Numbers[j])
				multiples[i] += 1.0;
		}
	}

#pragma omp parallel for schedule(dynamic, 2)
	for (int i = 0; i < Numbers.size(); i++)
		Numbers[i] *= static_cast<unsigned int>(multiples[i]);
}

// paralelni verze - OpenMP a guided delba prace
void test_openmp_guided()
{
	std::vector<double> multiples(Numbers.size());

#pragma omp parallel for schedule(guided)
	for (int i = 0; i < Numbers.size(); i++)
	{
		multiples[i] = 0;
		for (size_t j = 0; j < Numbers.size(); j++)
		{
			if (Numbers[i] == Numbers[j])
				multiples[i] += 1.0;
		}
	}

#pragma omp parallel for schedule(guided)
	for (int i = 0; i < Numbers.size(); i++)
		Numbers[i] *= static_cast<unsigned int>(multiples[i]);
}

// paralelni verze - OpenMP a guided delba prace, tady s datovym typem integer - nebyla by nahodou prace s integery rychlejsi, nez s doubly?
void test_openmp_guided_int()
{
	std::vector<int> multiples(Numbers.size());

#pragma omp parallel for schedule(guided)
	for (int i = 0; i < Numbers.size(); i++)
	{
		multiples[i] = 0;
		for (size_t j = 0; j < Numbers.size(); j++)
		{
			if (Numbers[i] == Numbers[j])
				multiples[i]++;
		}
	}

#pragma omp parallel for schedule(guided)
	for (int i = 0; i < Numbers.size(); i++)
		Numbers[i] *= multiples[i];
}

int main(int argc, char** argv)
{
	std::default_random_engine reng(123); // konstantni seed, aby byla cisla stale stejna mezi spustenimi programu
	std::uniform_int_distribution<unsigned int> dist(0, 1024);

	for (size_t i = 0; i < NumbersCount; i++)
		Numbers.push_back(dist(reng));

	NumbersCopy = Numbers;

	Do_Measure("Serial version", &test_serial);
	Do_Measure("Parallel (thread)", &test_parallel_thread);
	Do_Measure("Parallel (OpenMP, static)", &test_openmp_static);
	Do_Measure("Parallel (OpenMP, dynamic)", &test_openmp_dynamic);
	Do_Measure("Parallel (OpenMP, dynamic/gran)", &test_openmp_dynamic_gran);
	Do_Measure("Parallel (OpenMP, guided)", &test_openmp_guided);
	Do_Measure("Parallel (OpenMP, guided/int)", &test_openmp_guided_int);

	return 0;
}
