#include "patient.h"

#include <random>
#include <iostream>
#include <thread>
#include <chrono>

namespace {
	std::mt19937 gen(std::random_device{}());

	// distribuce jidel a obsahu cukru
	std::uniform_int_distribution<> meal_dist(120, 240); // 2 - 4 hodiny
	std::uniform_int_distribution<> meal_cho_dist(5, 60); // 5 - 60 g sacharidu v jidle

	// prevodni faktory
	constexpr double mgdl_to_mmol = 18.018;
	constexpr double iu_to_mu = 50.0;
}

// parametry modelu
namespace params {
	constexpr double k12 = 0.4;
	constexpr double kIQ = 0.6;
	constexpr double kId = 0.3;
	constexpr double kIabs = 0.35;
	constexpr double kD2Q = 0.4;
	constexpr double kEmp = 0.001;
	constexpr double Qthreshold = 250.0;
}

CVirtual_Patient::CVirtual_Patient() {
	mMins_To_Next_Meal = meal_dist(gen);
}

void CVirtual_Patient::Step() {
	TState current_state = mState;

	// toto je v podstate redukovana verze Hovorkova modelu

	const double k_emp = mState.Q1 > params::Qthreshold ? params::kEmp : 0.0;

	mState.Q1 += -params::k12 * (current_state.Q1 - current_state.Q2) - current_state.X * params::kIQ + params::kD2Q * current_state.D2 - k_emp * current_state.Q1;
	mState.Q2 += params::k12 * (current_state.Q1 - current_state.Q2);
	mState.I += params::kIabs * current_state.S1 - params::kId * current_state.I;
	mState.X += params::kId * current_state.I - params::kIQ * current_state.X;
	mState.S1 += -params::kIabs * current_state.S1;
	mState.D1 += -params::kD2Q * current_state.D1;
	mState.D2 += params::kD2Q * current_state.D1 - params::kD2Q * current_state.D2;

	mState.D1 = std::max(0.0, mState.D1);
	mState.D2 = std::max(0.0, mState.D2);
	mState.Q1 = std::max(0.0, mState.Q1);
	mState.Q2 = std::max(0.0, mState.Q2);
	mState.I = std::max(0.0, mState.I);
	mState.X = std::max(0.0, mState.X);
	mState.S1 = std::max(0.0, mState.S1);

	mCurrent_Time_Mins += 5;
	if (mMins_To_Next_Meal > 5) {
		mMins_To_Next_Meal -= 5;
	}
	else {
		const double cho = meal_cho_dist(gen) * 8;
		mState.D1 += cho;
		mMins_To_Next_Meal = meal_dist(gen);
	}

	// simulace zpozdeni
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

double CVirtual_Patient::Get_Current_Glucose() {
	return mState.Q1 / mgdl_to_mmol;
}

void CVirtual_Patient::Dose_Insulin(double dose) {
	mState.S1 += dose * iu_to_mu;
}
