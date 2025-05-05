#pragma once

class CVirtual_Controller {
	private:
		size_t mCurrent_Time = 0;
		size_t mLast_Dose_Time = 0;

	public:
		CVirtual_Controller();

		double Get_Control_Response(double input);
};
