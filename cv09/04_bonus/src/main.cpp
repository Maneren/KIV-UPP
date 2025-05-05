#include <iostream>

#include "patient.h"
#include "sensor.h"
#include "fir_filter.h"
#include "controller.h"

int main(int argc, char* argv[]) {

	/*
	 * Velmi jednoducha simulace pacienta s diabetem 1. typu
	 * 
	 * CVirtual_Patient je model pacienta - umime do nej davkovat inzulin a jidlo
	 *                  - model si sam vymysli, kdy se naji (nahodne)
	 *                  - inzulin se ale musi davat externe
	 *                  - model jde krokovat, abychom mohli simulovat plynuti casu
	 *                  - z modelu si vzdy muzeme vyzvednout aktualni hladinu glukozy
	 * 
	 * CVirtual_Sensor je model senzoru - umime z nej vyzvednout aktualni zmerenou hladinu glukozy
	 *                  - fakticky je to jenom kopie hodnoty z pacienta obohacena o sum
	 *                  - toto simuluje realny scenar, kdy senzor vzdy vnasi do mereni chybu
	 * 
	 * CFIR_Filter je FIR (Finite Impulse Response) filtr - v podstate jde o prumer poslednich N hodnot
	 *                  - toto je zase komponenta, ktera se obvykle objevi v realnych situacich na elementarni odstraneni sumu
	 *                  - samozrejme existuji lepsi metody odstraneni sumu
	 * 
	 * CVirtual_Controller je model ridiciho algoritmu - na zaklade prectene a prefiltrovane hodnoty urci, kolik se ma dat inzulinu
	 *                  - sam o sobe ma interne casovac, ktery nedovoli davkovat vzdy
	 *                  - vraci tedy bud 0 (tzn. nechceme davkovat nic) nebo cislo vetsi nez 0 znamenajici kolik jednotek inzulinu se ma podat
	 *                  - vystup se musi predat "do pacienta", tj. tvorime vlastne neco jako uzavrenou smycku (z teorie rizeni - closed loop, feedback loop)
	 */

	CVirtual_Patient patient;
	CVirtual_Sensor sensor;
	CFIR_Filter filter;
	CVirtual_Controller controller;

	for (size_t i = 0; i < 1000; i++) {
		patient.Step();

		std::cout << "Aktualni glykemie: " << patient.Get_Current_Glucose() << std::endl;

		filter.Add_Sample(sensor.Get_Glucose_Reading(patient.Get_Current_Glucose()));
		const double filtered_glucose = filter.Get_Average();

		double dose = controller.Get_Control_Response(filtered_glucose);
		if (dose > 0) {
			patient.Dose_Insulin(dose);
		}
	}

	/*
	 * Vas bonusovy ukol:
	 *   - pouzijte implementace vyse, ale s tim, ze kazda komponenta pobezi na jinem uzlu v MPI clusteru
	 *   - kazdy MPI uzel bude obstaravat jednu entitu a bude komunikovat s nasledujicim uzlem
	 *   - posledni uzel (ridici algoritmus) bude komunikovat s uzlem prvnim (pacient)
	 *     -> vytvarite tedy kruhovou virtualni topologii
	 * 
	 *   - pravidla pro komunikaci:
	 *     - pacient + senzor = standardni rezim komunikace (buffery, bez explicitni a vynucene synchronizace, tj. MPI_Send a _Recv)
	 *     - senzor + FIR filter = standardni
	 *     - FIR filter + ridici algoritmus = "ready" rezim, tj. ridici algoritmus musi uz cekat v MPI_Recv, kdyz FIR filter odesila data (najdete prislusnou variantu Send funkce!)
	 *                                      - pokud tam ridici algoritmus uz neceka, tak se data zahodi
	 *     - ridici algoritmus + pacient = synchronni rezim, tj. ridici algoritmus ceka, nez si pacient vyzvedne hodnotu (najdete prislusnou variantu Send funkce!)
	 */

	return 0;
}
