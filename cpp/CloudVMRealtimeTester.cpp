#include <iostream>
#include <fstream>
#include <csignal>
#include <cstring>
using namespace std;

#include <BetterExceptions.h>
#include <TimeMeasurements.h>
using namespace mutua::cpputils;


// the signal handling scheme: sigterm cause the measurements to stop and results to be dumped
static bool abortMeasurements = false;
void signalHandler(int signum) {
	if ((signum == SIGTERM) || (signum == SIGINT)) {
		cout << "SIGTERM received. Dumping results:" << endl;
		abortMeasurements = true;
	} else {
		cout << "Unknown signal #" << signum << " received. Ignoring..." << endl;
	}
}

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

	RealTimeMeasurements() {
		zeroAll();
	}

	void zeroAll() {
		memset(minutesOfAllHours,          0, sizeof minutesOfAllHours);
		memset(minutesOfEachHour,          0, sizeof minutesOfEachHour);
		memset(minutesOfEachHourOfEachDay, 0, sizeof minutesOfEachHourOfEachDay);
		memset(hourOfAllDays,              0, sizeof hourOfAllDays);
		memset(hourOfEachDay,              0, sizeof hourOfEachDay);
		memset(dayOfAllWeeks,              0, sizeof dayOfAllWeeks);
		memset(dayOfEachWeek,              0, sizeof dayOfEachWeek);
		memset(dayOfTheMonth,              0, sizeof dayOfTheMonth);
	}

};

// gaussian distribution module vars and their default values (may be overwriten by program arguments)
float              MAX_HANG_SECONDS                   = 36.0f;		// we are able to compute delays of up to this quantity, in seconds
unsigned long long DISTRIBUTION_TIME_PRECISION_FACTOR = 1'000;		// from ns to whatever resolution. 1000 * 1000 leads to ms time resolution
#define            GAUSSIAN_SLOTS_FORMULA               ((unsigned)((1'000*1'000*1'000.f/(float)DISTRIBUTION_TIME_PRECISION_FACTOR)*MAX_HANG_SECONDS)) // to be used to recalculate 'GAUSSIAN_SLOTS'
unsigned           GAUSSIAN_SLOTS                     = GAUSSIAN_SLOTS_FORMULA;
/** This struct is used to track the distribution of measured values */
struct DistributionTimeMeasurements {

	unsigned long long* gaussianTimes;
	unsigned            nGaussianTimesOverflows;

	DistributionTimeMeasurements() {
		gaussianTimes           = new unsigned long long[GAUSSIAN_SLOTS];
		zeroAll();
	}

	~DistributionTimeMeasurements() {
		delete[] gaussianTimes;
	}

	void zeroAll() {
		memset(gaussianTimes, 0, sizeof(unsigned long long) * GAUSSIAN_SLOTS);
		nGaussianTimesOverflows = 0;
	}

};

/** auxiliary function -- for each one of the 'nElements' of 'array', iterate over it calling 'callback(&array[n])' */
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
	                  unsigned long long            measurementDurationNS,
	                  bool                          verbose) {
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

		unsigned scaledElapsedTime = (unsigned)(elapsedTimeNS/DISTRIBUTION_TIME_PRECISION_FACTOR);
		if (scaledElapsedTime >= GAUSSIAN_SLOTS) {
			distributions->nGaussianTimesOverflows++;
		} else {
			distributions->gaussianTimes[scaledElapsedTime]++;
		}


		lastTimeNS = currentTimeNS;

	} while ( !abortMeasurements && ((currentTimeNS - startTimeNS) < measurementDurationNS) );

	if (verbose) {
		cout << "Measurement completed after " << ((currentTimeNS - startTimeNS) / 1'000'000'000) << " seconds." << endl;
	}

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

#define WRITE_1D_FILE(_fileName, _1dArray, _xElements) {                       \
	cout << "Writing '"s + (_fileName) + "'..."s << flush;                     \
	std::ofstream outfile(_fileName, std::ofstream::binary);                   \
	TRAVERSE1D(_1dArray, _xElements, outfile << x << "\t" << element << endl); \
	outfile.close();                                                           \
	cout << " Done." << endl << flush;                                         \
}

#define WRITE_2D_FILE(_fileName, _2dArray, _xElements, _yElements) {                                     \
	cout << "Writing '"s + (_fileName) + "'..."s << flush;                                               \
	std::ofstream outfile(_fileName, std::ofstream::binary);                                             \
	TRAVERSE2D(_2dArray, _xElements, _yElements, outfile << x << "\t" << y << "\t" << element << endl);  \
	outfile.close();                                                                                     \
	cout << " Done." << endl << flush;                                                                   \
}

void writeRealTimeMeasurements(string measurementName, RealTimeMeasurements *measurements) {

	WRITE_1D_FILE(measurementName+"_minutesOfAllHours", measurements->minutesOfAllHours,          MINUTES_IN_AN_HOUR);
	WRITE_2D_FILE(measurementName+"_minutesOfEachHour", measurements->minutesOfEachHour,          HOURS_IN_A_DAY,   MINUTES_IN_AN_HOUR);

	// cout << "\tminutesOfEachHourOfEachDay:" << endl;
	// TRAVERSE3D(measurements->minutesOfEachHourOfEachDay, DAYS_IN_A_MONTH,  HOURS_IN_A_DAY, MINUTES_IN_AN_HOUR, cout << "\t\t(" << x << ", " << y << ", " << z << "): " << element << "ns" << endl);

	WRITE_1D_FILE(measurementName+"_hourOfAllDays",     measurements->hourOfAllDays,              HOURS_IN_A_DAY);
	WRITE_2D_FILE(measurementName+"_hourOfEachDay",     measurements->hourOfEachDay,              DAYS_IN_A_MONTH,  HOURS_IN_A_DAY);
	WRITE_1D_FILE(measurementName+"_dayOfAllWeeks",     measurements->dayOfAllWeeks,              DAYS_IN_A_WEEK);
	WRITE_2D_FILE(measurementName+"_dayOfEachWeek",     measurements->dayOfEachWeek,              WEEKS_IN_A_MONTH, DAYS_IN_A_WEEK);
	WRITE_1D_FILE(measurementName+"_dayOfTheMonth",     measurements->dayOfTheMonth,              DAYS_IN_A_MONTH);
}

void writeDistributionMeasurements(DistributionTimeMeasurements *distributions) {

	string gaussianTimeUnit;
	switch (DISTRIBUTION_TIME_PRECISION_FACTOR) {
		case 1:
			gaussianTimeUnit = "ns";
			break;
		case 1'000:
			gaussianTimeUnit = "µs";
			break;
		case 1'000'000:
			gaussianTimeUnit = "ms";
			break;
		case 1'000'000'000:
			gaussianTimeUnit = "s";
			break;
		default:
			gaussianTimeUnit = "(unknown time unit -- factor = " + std::to_string(DISTRIBUTION_TIME_PRECISION_FACTOR) + ")";
	}

	cout << "nGaussianTimesOverflows (over " << GAUSSIAN_SLOTS << gaussianTimeUnit << "): " << distributions->nGaussianTimesOverflows << endl;

	// find the last measured time in the gaussian distribution to avoid dumping a bunch of zeroes
	unsigned lastGaussianSlot = GAUSSIAN_SLOTS;
	for (unsigned i=GAUSSIAN_SLOTS-1; i>=0; i--) {
		if (distributions->gaussianTimes[i] > 0) {
			lastGaussianSlot = i+1;
			break;
		}
	}

	WRITE_1D_FILE("gaussianTimes", distributions->gaussianTimes, lastGaussianSlot);
}

void writeResults(RealTimeMeasurements *worsts, RealTimeMeasurements *averages, DistributionTimeMeasurements *distributions) {

	cout << "Worst measurements:" << endl;
	writeRealTimeMeasurements("worsts", worsts);
	cout << "Average measurements:" << endl;
	writeRealTimeMeasurements("averages", averages);

	writeDistributionMeasurements(distributions);
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
int main(int argc, char *argv[]) {

	RealTimeMeasurements         *worsts;
	RealTimeMeasurements         *averages;
	RealTimeMeasurements         *numberOfMeasurements;
	DistributionTimeMeasurements *distributions;

	if (argc < 2) {
		cout << "CloudVMRealtimeTester: A tool to gather statistics and quantify the reactiveness of Virtual Machines, " << endl;
 		cout << "reporting if they may be trusted to execute hard, firm or soft Real-Time applications." << endl;
 		cout << endl;
 		cout << "Usage:" << endl;
		cout << "\tsudo sync; sudo nice -n -20 CloudVMRealtimeTester <seconds to measure> [time precision] [seconds to track on distribution accounting]" << endl;
		cout << "where:" << endl;
		cout << "\t'seconds to measure' is the number of seconds to do busy waiting measurements" << endl;
		cout << "\t'time precision' is optional and should one of: ns, µs (default), ms or s" << endl;
		cout << "\t'seconds to track on distribution accounting' is used to generate the gaussian distribution of the real-time" << endl;
		cout << "\t                                              offenses. On hard real-time, every loop should take the same time to" << endl;
		cout << "\t                                              complete, leading all measurements to fall into the same gaussian" << endl;
		cout << "\t                                              slot. The system will be less suitable for hard real-time purposes" << endl;
		cout << "\t                                              when more slots are needed. This option is used to allocate the" << endl;
		cout << "\t                                              necessarys slots to track delays up to that given number of seconds." << endl;
		cout << "\t                                              Use with care, observing your choice for 'time precision', since" << endl;
		cout << "\t                                              the memory needed is:" << endl;
		cout << "\t                                                - sizeof int * 1e9 * seconds to track for ns precision (~4 GiB/s)," << endl;
		cout << "\t                                                - sizeof int * 1e6 * seconds to track for µs precision (~4 MiB/s)," << endl;
		cout << "\t                                                - sizeof int * 1e3 * seconds to track for ms precision (~4 KiB/s)," << endl;
		cout << "\t                                                - sizeof int * 1e0 * seconds to track for second precision (~4b/s)." << endl;
 		cout << endl;
 		return 1;
	}

	// register the signals & handling function
	signal(SIGTERM, signalHandler);
	signal(SIGINT,  signalHandler);

	// parameter gathering
	//////////////////////

	unsigned long long numberOfSecondsToMeasure = atoi(argv[1]);
	unsigned long long timePrecisionFactor = 1'000'000;	// defaults to µs
	if (argc > 2) {
		// use the supplied precision
		if (strcmp(argv[2], "ns") == 0) {
			DISTRIBUTION_TIME_PRECISION_FACTOR = 1;
			MAX_HANG_SECONDS = 0.2f;
		} else if (strcmp(argv[2], "µs") == 0) {
			DISTRIBUTION_TIME_PRECISION_FACTOR = 1'000;
			MAX_HANG_SECONDS = 36;
		} else if (strcmp(argv[2], "ms") == 0) {
			DISTRIBUTION_TIME_PRECISION_FACTOR = 1'000'000;
			MAX_HANG_SECONDS = 60;
		} else if (strcmp(argv[2], "s") == 0) {
			DISTRIBUTION_TIME_PRECISION_FACTOR = 1'000'000'000;
			MAX_HANG_SECONDS = 3600;
		} else {
			cout << "Wrong value '"+std::string(argv[2])+"' for 'time precision' argument. Please consult usage.";
			return 1;
		}
	}
	if (argc > 3) {
		MAX_HANG_SECONDS = atof(argv[3]);
	}

	// recalculate the needed gaussian slots
	GAUSSIAN_SLOTS = GAUSSIAN_SLOTS_FORMULA;

	cout << "CloudVMRealtimeTester: Starting hard real-time measurements with parameters:" << endl;
	cout << "\t'seconds to measure'                                 : " + std::to_string(numberOfSecondsToMeasure) << endl;
	cout << "\tmax measurements per second ('time precision' factor): " + std::to_string(DISTRIBUTION_TIME_PRECISION_FACTOR) << endl;
	cout << "\t'seconds to track on distribution accounting'        : " + std::to_string(MAX_HANG_SECONDS) + " ("+std::to_string(GAUSSIAN_SLOTS)+" gaussian slots)"<< endl;
	cout << endl;

	cout << "Warming up for 5 seconds." << endl;
	cout << "Then, performing real measurements for up to " << numberOfSecondsToMeasure << " seconds or until a SIGTERM is received..." << endl << flush;
	worsts               = new RealTimeMeasurements();
	averages             = new RealTimeMeasurements();
	numberOfMeasurements = new RealTimeMeasurements();
	distributions        = new DistributionTimeMeasurements();
	realTimeTestLoop(worsts, averages, numberOfMeasurements, distributions, 1'000'000'000ll*5ll, false);
	worsts->zeroAll();
	averages->zeroAll();
	numberOfMeasurements->zeroAll();
	distributions->zeroAll();

	// the real test
	realTimeTestLoop(worsts, averages, numberOfMeasurements, distributions, 1'000'000'000ll*numberOfSecondsToMeasure, true);

	// results
	writeResults(worsts, averages, distributions);

	delete worsts;
	delete averages;
	delete numberOfMeasurements;
	delete distributions;

	return 0;
}