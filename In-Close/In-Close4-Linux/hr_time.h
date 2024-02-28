// Ported to run on both Windows and Linux
// Linux version uses OpenMP's omp_get_wtime() function

// Last Change : 23rd January 2016

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef HR_TIME_H
#define HR_TIME_H

typedef struct {
#ifdef _WIN32
    LARGE_INTEGER start;
    LARGE_INTEGER stop;
#else
	double start;
	double stop;
#endif
} stopWatch;

class CStopWatch {

private:
	stopWatch timer;
#ifdef _WIN32
	LARGE_INTEGER frequency;
	double LIToSecs( LARGE_INTEGER & L);
#else
	double frequency;
	double LIToSecs( double & L);
#endif
public:
	CStopWatch();
	void startTimer( );
	void stopTimer( );
	double getElapsedTime();
};

#endif  // HR_TIME
