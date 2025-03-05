#pragma once

#include <mutex>

/*
 * POZOR: tato implementace semaforu nebude fungovat - zakladni predpoklad je, ze metody P a V jsou atomicke
 *        tady ale nic atomicitu nezarucuje!
 */
class Semaphore {
	private:
		int counter;

	public:
		Semaphore(int init) : counter(init) {}

		void P(int cnt) {
			while (counter < cnt) {
				;
			}

			counter -= cnt;
		}

		void V(int cnt) {
			counter += cnt;
		}

		int Get() {
			return counter;
		}
};
