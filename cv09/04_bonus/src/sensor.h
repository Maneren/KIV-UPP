#pragma once

#include <random>

class CVirtual_Sensor {
	private:
		std::default_random_engine mRandom_Engine;
		std::uniform_real_distribution<double> mNoise_Distribution;

	public:
		CVirtual_Sensor();

		double Get_Glucose_Reading(double glucose);
};
