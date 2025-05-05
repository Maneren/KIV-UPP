#include <iomanip>
#include <iostream>
#include <mpi.h>
#include <random>

constexpr size_t MatrixSize = 32;

std::vector<double> Matrix_A, Matrix_B;

// vygeneruje matici, pokazde bude matice stejna diky dodavanemu seedu
void Generate_Matrix(std::vector<double> &mat, size_t size, int seed) {
  std::default_random_engine reng(seed);
  std::uniform_real_distribution<double> rdist(0.0, 2.0);

  mat.resize(size * size);
  for (size_t i = 0; i < mat.size(); i++) {
    mat[i] = rdist(reng);
  }
}

int main(int argc, char **argv) {
  size_t matrix_size;

  // inicializace MPI, prihlaseni do sezeni
  MPI_Init(&argc, &argv);

  // zjistime si nas rank (adresu, identifikator) a velikost skupiny
  int rank, total;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &total);

  // jen root vygeneruje matice
  if (rank == 0) {
    std::cout << "[0] root generating matrixes..." << std::endl;
    matrix_size = MatrixSize;
    Generate_Matrix(Matrix_A, MatrixSize, 123);
    Generate_Matrix(Matrix_B, MatrixSize, 456);
  }

  // kolektivni operace - volaji vsichni, fakticky ale jen jeden odesila (ID =
  // 0), ostatni prijimaji root rozposle vsem velikost matice
  MPI_Bcast(&matrix_size, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);

  // jen non-root uzly si alokuji pamet (root uz ji davno ma, bude ji ostatnim
  // plnit)
  if (rank != 0) {
    std::cout << "[" << rank
              << "] non-root allocating memory (size = " << matrix_size
              << ")..." << std::endl;
    Matrix_A.resize(matrix_size * matrix_size);
    Matrix_B.resize(matrix_size * matrix_size);
  }

  // root rozposila vsem obsah matice A a B
  MPI_Bcast(Matrix_A.data(), matrix_size * matrix_size, MPI_DOUBLE, 0,
            MPI_COMM_WORLD);
  MPI_Bcast(Matrix_B.data(), matrix_size * matrix_size, MPI_DOUBLE, 0,
            MPI_COMM_WORLD);

  // kazdy uzel si vypocita, ktere radky vysledne matice bude zpracovavat
  const size_t start_row = rank * (matrix_size / total);
  const size_t end_row = (rank + 1) * (matrix_size / total);

  // alokuje si vyslednou podmatici
  std::vector<double> Matrix_C(matrix_size * (end_row - start_row));

  std::cout << "[" << rank << "] work distributed, multiplying target rows "
            << start_row << " through " << end_row << std::endl;

  // vynasobi kusy matice
  for (size_t y = start_row; y < end_row; y++) {
    for (size_t x = 0; x < matrix_size; x++) {
      Matrix_C[(y - start_row) * matrix_size + x] = 0;
      for (size_t it = 0; it < matrix_size; it++) {
        Matrix_C[(y - start_row) * matrix_size + x] +=
            Matrix_A[y * matrix_size + it] * Matrix_B[it * matrix_size + x];
      }
    }
  }

  std::cout << "[" << rank << "] finished" << std::endl;

  // explicitni bariera uplne nutna neni
  MPI_Barrier(MPI_COMM_WORLD);

  // jen root prijima vysledky
  if (rank == 0) {
    // svou vyslednou matici roztahne na plnou velikost (jeho vysledek v ni
    // zustane)
    Matrix_C.resize(matrix_size * matrix_size);

    const size_t elem_count = (end_row - start_row) * matrix_size;

    // od vsech ostatnich ID prijme jejich vysledky a zaradi je na spravnou
    // pozici ve vysledne matici
    for (int i = 1; i < total; i++) {
      MPI_Recv(Matrix_C.data() + i * elem_count, elem_count, MPI_DOUBLE, i, 0,
               MPI_COMM_WORLD, NULL);
    }
  } else { // ostatni vysilaji
    MPI_Send(Matrix_C.data(), Matrix_C.size(), MPI_DOUBLE, 0, 0,
             MPI_COMM_WORLD);
  }

  // jen root vypise kompletni vyslednou matici
  if (rank == 0) {
    for (size_t y = 0; y < matrix_size; y++) {
      for (size_t x = 0; x < matrix_size; x++) {
        std::cout << std::fixed << std::setprecision(2)
                  << Matrix_C[y * matrix_size + x];
        if (x != matrix_size - 1) {
          std::cout << " ";
        } else {
          std::cout << std::endl;
        }
      }
    }
  }

  // ukonceni prace s MPI, odhlaseni ze skupiny
  MPI_Finalize();

  return 0;
}
