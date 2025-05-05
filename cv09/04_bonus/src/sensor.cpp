#include "sensor.h"

CVirtual_Sensor::CVirtual_Sensor() {
	mRandom_Engine.seed(std::random_device{}());
	mNoise_Distribution = std::uniform_real_distribution<double>(-0.5, 0.5);
}

double CVirtual_Sensor::Get_Glucose_Reading(double glucose) {
	// pouze pridame bily sum k hodnoty glukozy
	const double noise = mNoise_Distribution(mRandom_Engine);
	return glucose + noise;
}
