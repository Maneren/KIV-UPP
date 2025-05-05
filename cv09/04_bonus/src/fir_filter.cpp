#include "fir_filter.h"

CFIR_Filter::CFIR_Filter() {
	mBuffer_Index = 0;
	mValid_Values = 0;
}

void CFIR_Filter::Add_Sample(double sample) {
	mBuffer[mBuffer_Index] = sample;
	mBuffer_Index = (mBuffer_Index + 1) % mBuffer.size();

	mValid_Values = std::min(mValid_Values + 1, mBuffer.size());
}

double CFIR_Filter::Get_Average() const {
	if (mValid_Values == 0) {
		return 0.0;
	}

	// zprumerujeme vsechny platne hodnoty
	double sum = 0.0;
	for (size_t i = 0; i < mValid_Values; ++i) {
		sum += mBuffer[i];
	}

	return sum / mValid_Values;
}
