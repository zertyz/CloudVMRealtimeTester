#include <iostream>
using namespace std;

#include <BetterExceptions.h>
#include <TimeMeasurements.h>
using namespace mutua::cpputils;


constexpr unsigned HOURS_IN_A_DAY   = 24;
constexpr unsigned DAYS_IN_A_MONTH  = 31;
constexpr unsigned DAYS_IN_A_WEEK   = 7;
constexpr unsigned WEEKS_IN_A_MONTH = 5;
/** This struct is used to measure worst and average times */
struct RealTimeMeasurements {

	unsigned long long hourOfAllDays[HOURS_IN_A_DAY];
	unsigned long long hourOfEachDay[DAYS_IN_A_MONTH][HOURS_IN_A_DAY];
	unsigned long long dayOfAllWeeks[DAYS_IN_A_WEEK];
	unsigned long long dayOfEachWeek[WEEKS_IN_A_MONTH][DAYS_IN_A_WEEK];
	unsigned long long dayOfTheMonth[DAYS_IN_A_MONTH];
	unsigned long long numberOfMeasurements;

	RealTimeMeasurements()
			: hourOfAllDays{0}
			, hourOfEachDay{0}
			, dayOfAllWeeks{0}
			, dayOfEachWeek{0}
			, dayOfTheMonth{0}
			, numberOfMeasurements(0) {}

};

void realTimeTestLoop(RealTimeMeasurements &worsts, RealTimeMeasurements &averages, unsigned long long measurementDurationNS) {
	unsigned long long startTimeNS   = TimeMeasurements::getMonotonicRealTimeNS();
	unsigned long long lastTimeNS    = startTimeNS;
	unsigned long long currentTimeNS;

	do {

		unsigned hourOfDay   =   ( (lastTimeNS / 1000000000ll) / 3600ll ) % 24ll;
		unsigned dayOfWeek   = ( ( (lastTimeNS / 1000000000ll) / 3600ll ) / 24ll ) % 7ll;
		unsigned dayOfMonth  = ( ( (lastTimeNS / 1000000000ll) / 3600ll ) / 24ll ) % 31ll;
		unsigned weekOfMonth = dayOfMonth % 7;

		currentTimeNS = TimeMeasurements::getMonotonicRealTimeNS();
		unsigned long long elapsedTimeNS = currentTimeNS - lastTimeNS;

#define COMPUTE_WORST_MEASUREMENT(_var)     \
		if (elapsedTimeNS > _var) {         \
			_var = elapsedTimeNS;           \
		}                                   \

		COMPUTE_WORST_MEASUREMENT(worsts.hourOfAllDays[hourOfDay]);
		COMPUTE_WORST_MEASUREMENT(worsts.hourOfEachDay[dayOfMonth][hourOfDay]);
		COMPUTE_WORST_MEASUREMENT(worsts.dayOfAllWeeks[dayOfWeek]);
		COMPUTE_WORST_MEASUREMENT(worsts.dayOfEachWeek[weekOfMonth][dayOfWeek]);
		COMPUTE_WORST_MEASUREMENT(worsts.dayOfTheMonth[dayOfMonth]);
		worsts.numberOfMeasurements++;

#define COMPUTE_AVERAGE_MEASUREMENT(_var)  \
		_var += elapsedTimeNS;             \

		COMPUTE_AVERAGE_MEASUREMENT(averages.hourOfAllDays[hourOfDay]);
		COMPUTE_AVERAGE_MEASUREMENT(averages.hourOfEachDay[dayOfMonth][hourOfDay]);
		COMPUTE_AVERAGE_MEASUREMENT(averages.dayOfAllWeeks[dayOfWeek]);
		COMPUTE_AVERAGE_MEASUREMENT(averages.dayOfEachWeek[weekOfMonth][dayOfWeek]);
		COMPUTE_AVERAGE_MEASUREMENT(averages.dayOfTheMonth[dayOfMonth]);
		averages.numberOfMeasurements++;

#undef COMPUTE_WORST_MEASUREMENT
#undef COMPUTE_AVERAGE_MEASUREMENT

		lastTimeNS = currentTimeNS;

	} while ((currentTimeNS - startTimeNS) < measurementDurationNS);

	// compute averages
	for (unsigned i=0; i<HOURS_IN_A_DAY; i++) {
		averages.hourOfAllDays[i] /= averages.numberOfMeasurements; 
	}

}


void outputRealTimeMeasurements(RealTimeMeasurements &measurements) {
	cout << "\thourOfAllDays:\n";
	for (unsigned i=0; i<HOURS_IN_A_DAY; i++) {
		cout << "\t\t" << i << ": " << measurements.hourOfAllDays[i] << "ns\n";
	}
}

void outputResults(RealTimeMeasurements &worsts, RealTimeMeasurements &averages) {
	cout << "Worst measurements:\n";
	outputRealTimeMeasurements(worsts);
	cout << "Average measurements:\n";
	outputRealTimeMeasurements(averages);
}


/** <pre>
 * CloudVMRealtimeTester.cpp
 * =========================
 * created by luiz, Jan 5, 2019
 *
 * A tool to gather statistics and quantify the reactiveness of Virtual Machines, reporting if they may be
 * trusted to execute hard, firm or soft Real-Time applications.
 *
 * For more docs, please see: ...
 *
*/
int main() {
	RealTimeMeasurements worsts;
	RealTimeMeasurements averages;
	realTimeTestLoop(worsts, averages, 1000000000ll * 15ll);
	outputResults(worsts, averages);
}