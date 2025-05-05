#pragma once

#include <array>

class CFIR_Filter {
	private:
		std::array<double, 5> mBuffer{ 0.0 };
		size_t mBuffer_Index = 0;

		size_t mValid_Values = 0;

	public:
		CFIR_Filter();
		
		void Add_Sample(double sample);

		double Get_Average() const;
};
