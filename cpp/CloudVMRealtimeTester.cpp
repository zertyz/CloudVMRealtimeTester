#include <iostream>
using namespace std;

#include <BetterExceptions.h>
#include <TimeMeasurements.h>
using namespace mutua::cpputils;


constexpr unsigned MINUTES_IN_AN_HOUR = 60;
constexpr unsigned HOURS_IN_A_DAY     = 24;
constexpr unsigned DAYS_IN_A_MONTH    = 31;
constexpr unsigned DAYS_IN_A_WEEK     = 7;
constexpr unsigned WEEKS_IN_A_MONTH   = 5;
/** This struct is used to store worst and average times as well as the number of measurements */
struct RealTimeMeasurements {

	unsigned long long minutesOfAllHours[MINUTES_IN_AN_HOUR];
	unsigned long long minutesOfEachHour[HOURS_IN_A_DAY][MINUTES_IN_AN_HOUR];
	unsigned long long minutesOfEachHourOfEachDay[DAYS_IN_A_MONTH][HOURS_IN_A_DAY][MINUTES_IN_AN_HOUR];
	unsigned long long hourOfAllDays[HOURS_IN_A_DAY];
	unsigned long long hourOfEachDay[DAYS_IN_A_MONTH][HOURS_IN_A_DAY];
	unsigned long long dayOfAllWeeks[DAYS_IN_A_WEEK];
	unsigned long long dayOfEachWeek[WEEKS_IN_A_MONTH][DAYS_IN_A_WEEK];
	unsigned long long dayOfTheMonth[DAYS_IN_A_MONTH];

	RealTimeMeasurements()
			: minutesOfAllHours{0}
			, minutesOfEachHour{0}
			, minutesOfEachHourOfEachDay{0}
			, hourOfAllDays{0}
			, hourOfEachDay{0}
			, dayOfAllWeeks{0}
			, dayOfEachWeek{0}
			, dayOfTheMonth{0} {}

};

constexpr unsigned long long MAX_HANG_SECONDS                    = 3600;			// we are able to compute delays of up to this quantity, in seconds
constexpr unsigned long long DISTRIBUTION_TIME_RESOLUTION_FACTOR = 1'000 * 1'000;	// from ns to whatever resolution. 1000 * 1000 leads to ms time resolution
constexpr unsigned long long GAUSSIAN_MS_SLOTS                   = (1'000*1'000*1'000/DISTRIBUTION_TIME_RESOLUTION_FACTOR)*MAX_HANG_SECONDS; // ns / resolution * max_hang_secs
/** This struct is used to track the distribution of measured values */
struct DistributionTimeMeasurements {

	unsigned long long gaussianMS[GAUSSIAN_MS_SLOTS];
	unsigned           nGaussianMSOverflows; 

	DistributionTimeMeasurements()
			: gaussianMS{0}
			, nGaussianMSOverflows(0) {}

};

/** auxiliary function -- for each 'nElements' of 'array', iterate over it calling 'callback(&array[n])' */
void traverseArray(unsigned long long *array, unsigned nElements, void (&callback) (unsigned long long &element, unsigned n)) {
	for (unsigned n=0; n<nElements; n++) {
		callback(array[n], n);
	}
}

#define TRAVERSE1D(_1dArray, _xElements, _expression) \
	for (unsigned x=0; x<_xElements; x++) {           \
		auto &element = _1dArray[x];                  \
		_expression;                                  \
	}                                                 \

#define TRAVERSE2D(_2dArray, _xElements, _yElements, _expression) \
	for (unsigned x=0; x<_xElements; x++) {                       \
		for (unsigned y=0; y<_yElements; y++) {                   \
			auto &element = _2dArray[x][y];                       \
		    _expression;                                          \
		}                                                         \
	}                                                             \

#define TRAVERSE3D(_3dArray, _xElements, _yElements, _zElements, _expression) \
	for (unsigned x=0; x<_xElements; x++) {                                   \
		for (unsigned y=0; y<_yElements; y++) {                               \
			for (unsigned z=0; z<_zElements; z++) {                           \
				auto &element = _3dArray[x][y][z];                            \
		    	_expression;                                                  \
			}                                                                 \
		}                                                                     \
	}                                                                         \

void realTimeTestLoop(RealTimeMeasurements         *worsts,
	                  RealTimeMeasurements         *averages,
	                  RealTimeMeasurements         *numberOfMeasurements,
	                  DistributionTimeMeasurements *distributions,
	                  unsigned long long            measurementDurationNS) {
	unsigned long long startTimeNS   = TimeMeasurements::getMonotonicRealTimeNS();
	unsigned long long lastTimeNS    = startTimeNS;
	unsigned long long currentTimeNS;

	do {

		unsigned minuteOfHour =   ( (lastTimeNS / 1000000000ll) / 60ll ) % 60ll;
		unsigned hourOfDay    =   ( (lastTimeNS / 1000000000ll) / 3600ll ) % 24ll;
		unsigned dayOfWeek    = ( ( (lastTimeNS / 1000000000ll) / 3600ll ) / 24ll ) % 7ll;
		unsigned dayOfMonth   = ( ( (lastTimeNS / 1000000000ll) / 3600ll ) / 24ll ) % 31ll;
		unsigned weekOfMonth  = dayOfMonth % 7;

		currentTimeNS = TimeMeasurements::getMonotonicRealTimeNS();
		unsigned long long elapsedTimeNS = currentTimeNS - lastTimeNS;


		// compute time measurements
		////////////////////////////

#define COMPUTE_MEASUREMENTS(_var)                      \
		/* Compute worst measurement */                 \
		if (elapsedTimeNS > worsts->_var) {             \
			worsts->_var = elapsedTimeNS;               \
		}                                               \
		/* Compute averages & number of measurements */ \
		averages->_var             += elapsedTimeNS;    \
		numberOfMeasurements->_var += 1;                \

		COMPUTE_MEASUREMENTS(minutesOfAllHours[minuteOfHour]);
		COMPUTE_MEASUREMENTS(minutesOfEachHour[hourOfDay][minuteOfHour]);
		COMPUTE_MEASUREMENTS(minutesOfEachHourOfEachDay[dayOfMonth][hourOfDay][minuteOfHour]);
		COMPUTE_MEASUREMENTS(hourOfAllDays[hourOfDay]);
		COMPUTE_MEASUREMENTS(hourOfEachDay[dayOfMonth][hourOfDay]);
		COMPUTE_MEASUREMENTS(dayOfAllWeeks[dayOfWeek]);
		COMPUTE_MEASUREMENTS(dayOfEachWeek[weekOfMonth][dayOfWeek]);
		COMPUTE_MEASUREMENTS(dayOfTheMonth[dayOfMonth]);

#undef COMPUTE_MEASUREMENTS


		// compute distribution measurements
		////////////////////////////////////

		unsigned scaledElapsedTime = (unsigned)(elapsedTimeNS/DISTRIBUTION_TIME_RESOLUTION_FACTOR);
		if (scaledElapsedTime >= GAUSSIAN_MS_SLOTS) {
			distributions->nGaussianMSOverflows++;
		} else {
			distributions->gaussianMS[scaledElapsedTime]++;
		}


		lastTimeNS = currentTimeNS;

	} while ((currentTimeNS - startTimeNS) < measurementDurationNS);

	// compute averages
	TRAVERSE1D(averages->minutesOfAllHours,          MINUTES_IN_AN_HOUR,                                   auto n = numberOfMeasurements->minutesOfAllHours[x];                if (n > 0) element /= n);
	TRAVERSE2D(averages->minutesOfEachHour,          HOURS_IN_A_DAY,   MINUTES_IN_AN_HOUR,                 auto n = numberOfMeasurements->minutesOfEachHour[x][y];             if (n > 0) element /= n);
	TRAVERSE3D(averages->minutesOfEachHourOfEachDay, DAYS_IN_A_MONTH,  HOURS_IN_A_DAY, MINUTES_IN_AN_HOUR, auto n = numberOfMeasurements->minutesOfEachHourOfEachDay[x][y][z]; if (n > 0) element /= n);
	TRAVERSE1D(averages->hourOfAllDays,              HOURS_IN_A_DAY,                                       auto n = numberOfMeasurements->hourOfAllDays[x];                    if (n > 0) element /= n);
	TRAVERSE2D(averages->hourOfEachDay,              DAYS_IN_A_MONTH,  HOURS_IN_A_DAY,                     auto n = numberOfMeasurements->hourOfEachDay[x][y];                 if (n > 0) element /= n);
	TRAVERSE1D(averages->dayOfAllWeeks,              DAYS_IN_A_WEEK,                                       auto n = numberOfMeasurements->dayOfAllWeeks[x];                    if (n > 0) element /= n);
	TRAVERSE2D(averages->dayOfEachWeek,              WEEKS_IN_A_MONTH, DAYS_IN_A_WEEK,                     auto n = numberOfMeasurements->dayOfEachWeek[x][y];                 if (n > 0) element /= n);
	TRAVERSE1D(averages->dayOfTheMonth,              DAYS_IN_A_MONTH,                                      auto n = numberOfMeasurements->dayOfTheMonth[x];                    if (n > 0) element /= n);

}

void outputRealTimeMeasurements(RealTimeMeasurements *measurements) {

	cout << "\tminutesOfAllHours:\n";
	TRAVERSE1D(measurements->minutesOfAllHours,          MINUTES_IN_AN_HOUR,                                   cout << "\t\t" << x << ": " << element << "ns\n");

	cout << "\tminutesOfEachHour:\n";
	TRAVERSE2D(measurements->minutesOfEachHour,          HOURS_IN_A_DAY,   MINUTES_IN_AN_HOUR,                 cout << "\t\t(" << x << ", " << y << "): " << element << "ns\n");

	cout << "\tminutesOfEachHourOfEachDay:\n";
	TRAVERSE3D(measurements->minutesOfEachHourOfEachDay, DAYS_IN_A_MONTH,  HOURS_IN_A_DAY, MINUTES_IN_AN_HOUR, cout << "\t\t(" << x << ", " << y << ", " << z << "): " << element << "ns\n");

	cout << "\thourOfAllDays:\n";
	TRAVERSE1D(measurements->hourOfAllDays,              HOURS_IN_A_DAY,                                       cout << "\t\t" << x << ": " << element << "ns\n");

	cout << "\thourOfEachDay:\n";
	TRAVERSE2D(measurements->hourOfEachDay,              DAYS_IN_A_MONTH,  HOURS_IN_A_DAY,                     cout << "\t\t(" << x << ", " << y << "): " << element << "ns\n");

	cout << "\tdayOfAllWeeks:\n";
	TRAVERSE1D(measurements->dayOfAllWeeks,              DAYS_IN_A_WEEK,                                       cout << "\t\t" << x << ": " << element << "ns\n");

	cout << "\tdayOfEachWeek:\n";
	TRAVERSE2D(measurements->dayOfEachWeek,              WEEKS_IN_A_MONTH, DAYS_IN_A_WEEK,                     cout << "\t\t(" << x << ", " << y << "): " << element << "ns\n");

	cout << "\tdayOfTheMonth:\n";
	TRAVERSE1D(measurements->dayOfTheMonth,              DAYS_IN_A_MONTH,                                      cout << "\t\t" << x << ": " << element << "ns\n");
}

void outputResults(RealTimeMeasurements *worsts, RealTimeMeasurements *averages, DistributionTimeMeasurements *distributions) {
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
	RealTimeMeasurements         *worsts               = new RealTimeMeasurements();
	RealTimeMeasurements         *averages             = new RealTimeMeasurements();
	RealTimeMeasurements         *numberOfMeasurements = new RealTimeMeasurements();
	DistributionTimeMeasurements *distributions        = new DistributionTimeMeasurements();
	realTimeTestLoop(worsts, averages, numberOfMeasurements, distributions, 1000000000ll /* * 3600ll */ * 24ll);
	outputResults(worsts, averages, distributions);
}