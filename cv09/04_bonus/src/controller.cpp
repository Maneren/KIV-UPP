#include "controller.h"

#include <thread>
#include <chrono>

CVirtual_Controller::CVirtual_Controller() {
	//
}

double CVirtual_Controller::Get_Control_Response(double input) {
	mCurrent_Time += 5;

	// simulace zpozdeni
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	// davkujeme nanejvys jednou za pul hodiny simulovaneho casu
	if (mCurrent_Time - mLast_Dose_Time > 30) {

		// pokud je glykemie vysoka, davkujeme
		if (input > 7.0) {
			mLast_Dose_Time = mCurrent_Time;

			// tohle je hodne naivni vypocet
			return (input - 7.0) * 0.35;
		}
	}

	return 0.0;
}
