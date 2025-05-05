#pragma once

#include <string>
#include <vector>
#include <iostream>

/*
 * Hodne hloupy model metabolismu pacienta s diabetem 1. typu
 */
class CVirtual_Patient {
	private:
		// aktualni cas
		size_t mCurrent_Time_Mins = 0;

		struct TState {
			double Q1 = 300;
			double Q2 = 200;
			double I = 0;
			double X = 0;
			double S1 = 0;
			double D1 = 0;
			double D2 = 0;
		};

		// aktualni stav pacienta
		TState mState;

		// cas do dalsiho jidla
		size_t mMins_To_Next_Meal = 0;

	public:
		CVirtual_Patient();

		// provede krok v simulaci
		void Step();

		// zjisti aktualni glykemii
		double Get_Current_Glucose();

		// davkuje inzulin
		void Dose_Insulin(double dose);
};
