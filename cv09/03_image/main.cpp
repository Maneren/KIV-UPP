#include <iostream>
#include <mpi.h>
#include <cstdint>
#include <fstream>
#include <vector>
#include <array>

// velikost hlavicky BMP (preskakujeme)
constexpr size_t BMP_Header_Size = 0x36;
// sirka obrazku
constexpr size_t BMP_Width = 512;
// vyska obrazku
constexpr size_t BMP_Height = 512;
// pocet pixelu
constexpr size_t BMP_Pixel_Count = BMP_Width * BMP_Height;
// velikost obsahu obrazku v bajtech
constexpr size_t BMP_Contents_Size = BMP_Pixel_Count * 3;

// ulozeny header BMP (budeme ho jen znovu ukladat)
std::array<uint8_t, BMP_Header_Size> BMPHeader;

int main(int argc, char** argv) {
	MPI_Init(&argc, &argv);

	int rank, total;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &total);

	// vypocteme rovnou velikost prace
	const size_t WorkPortion = BMP_Pixel_Count / total;

	// sem ulozime obrazek
	std::vector<uint8_t> rawImage(BMP_Contents_Size);

	// root nacte obrazek cely
	if (rank == 0) {
		std::ifstream imgfile("test.bmp", std::ios::binary | std::ios::in);
		if (!imgfile.is_open()) {
			return 1;
		}

		imgfile.read((char*)BMPHeader.data(), BMP_Header_Size);
		imgfile.read((char*)rawImage.data(), BMP_Contents_Size);
	}

	// vytvorime datovy typ pro RGB pixel
	MPI_Datatype rgb_type;
	int lengths[3] = {1, 1, 1};
	MPI_Aint displacements[3] = {0, 1, 2};
	MPI_Datatype types[3] = { MPI_BYTE, MPI_BYTE, MPI_BYTE	};

	MPI_Type_create_struct(3, lengths, displacements, types, &rgb_type);
	MPI_Type_commit(&rgb_type);

	// rozposilame casti obrazku vsem procesum - Scatter
	MPI_Scatter(rawImage.data(), WorkPortion, rgb_type, rawImage.data() + (rank * WorkPortion * 3), WorkPortion, rgb_type, 0, MPI_COMM_WORLD);

	// spocitame, odkud kam nas bude zajimat obrazek
	const size_t WorkStart = rank * WorkPortion;
	const size_t WorkEnd = (rank + 1) * WorkPortion;

	// provedeme inverzi pixelu
	for (int i = WorkStart * 3; i < WorkEnd * 3; i += 3) {
		rawImage[i + 0] = 255 - rawImage[i + 0];
		rawImage[i + 1] = 255 - rawImage[i + 1];
		rawImage[i + 2] = 255 - rawImage[i + 2];
	}

	// vsem ostatnim rozposilame vsechny casti obrazku
	MPI_Allgather(rawImage.data() + (rank * WorkPortion * 3), WorkPortion, rgb_type, rawImage.data(), WorkPortion, rgb_type, MPI_COMM_WORLD);

	// najdeme maximalni hodnotu kazde slozky
	uint8_t maxvals[3] = {1, 1, 1};
	for (int i = WorkStart * 3; i < WorkEnd * 3; i += 3) {
		for (size_t c = 0; c < 3; c++) {
			if (rawImage[i + c] > maxvals[c])
				maxvals[c] = rawImage[i + c];
		}
	}

	// a vymenime si se vsemi informace o maximalnich hodnotach + je redukujeme; vysledkem je globalni shoda na maximalnich hodnotach prvku
	uint8_t glob_max[3];
	MPI_Allreduce(&maxvals, &glob_max, 3, MPI_BYTE, MPI_MAX, MPI_COMM_WORLD); 

	// vypocteme faktory zvetseni barevnych slozek
	double factors[3] = { 255.0 / (double)glob_max[0], 255.0 / (double)glob_max[1], 255.0 / (double)glob_max[2] };
	// a transformujeme obrazek
	for (int i = WorkStart * 3; i < WorkEnd * 3; i += 3) {
		for (size_t c = 0; c < 3; c++) {
			rawImage[i + c] = (uint8_t)((double)rawImage[i + c] * factors[c]);
		}
	}

	// nechame vse ulozit do roota
	MPI_Gather(rawImage.data() + (rank * WorkPortion * 3), WorkPortion, rgb_type, rawImage.data(), WorkPortion, rgb_type, 0, MPI_COMM_WORLD);

	// a root nasledne ulozi cely obrazek do souboru
	if (rank == 0) {
		std::ofstream out("out.bmp", std::ios::binary | std::ios::out);
		out.write((char*)BMPHeader.data(), BMP_Header_Size);
		out.write((char*)rawImage.data(), BMP_Contents_Size);
	}

	MPI_Finalize();

	return 0;
}
