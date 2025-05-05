#include <iostream>
#include <mpi.h>
#include <random>
#include <vector>
#include <algorithm>
#include <numeric>

constexpr size_t NumbersCount = 1'000'000;

void Fill_Vector(std::vector<double>& target, size_t num, int seed)
{
	std::default_random_engine reng(seed);
	std::uniform_real_distribution<double> rdist(0.0, 1.0);

	target.resize(num);
	for (size_t i = 0; i < num; i++)
		target[i] = rdist(reng);
}

int main(int argc, char** argv)
{
	MPI_Init(&argc, &argv);

	int rank, total;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &total);

	size_t vecSize;
	std::vector<double> workVector;

	double check = 0.0;

	if (rank == 0)
	{
		std::cout << "[" << rank << "]: generating values..." << std::endl;
		Fill_Vector(workVector, NumbersCount, 123);
		vecSize = NumbersCount;
	}

	std::cout << "[" << rank << "]: waiting for size broadcast" << std::endl;

	MPI_Bcast(&vecSize, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);

	const size_t workPortion = vecSize / total;

	if (rank != 0)
	{
		std::cout << "[" << rank << "]: non-root allocating memory" << std::endl;
		workVector.resize(workPortion);
	}

	std::cout << "[" << rank << "]: waiting for scatter operation" << std::endl;

	MPI_Scatter(workVector.data(), workPortion, MPI_DOUBLE, workVector.data(), workPortion, MPI_DOUBLE, 0, MPI_COMM_WORLD);

	// kvuli rootovi - ten ma celou praci, timhle zbytek prace ztrati
	workVector.resize(workPortion);

	std::cout << "[" << rank << "]: calculating local sum..." << std::endl;
	double res = std::accumulate(workVector.begin(), workVector.end(), 0.0);

	std::cout << "[" << rank << "]: done. Sum = " << res << std::endl;

	double globalSum = 0;

	std::cout << "[" << rank << "]: reducing to global sum to root" << std::endl;

	MPI_Reduce(&res, &globalSum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

	if (rank == 0)
		std::cout << "[" << rank << "]: finished! Global sum = " << globalSum << std::endl;

	MPI_Finalize();
	return 0;
}
