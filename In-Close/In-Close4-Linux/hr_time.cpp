// Ported to run on both Windows and Linux
// Linux version uses OpenMP's omp_get_wtime() function

// Last Change : 23rd January 2016

#include "stdafx.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <omp.h>
#endif

//#ifndef hr_timer
#include "hr_time.h"
//#define hr_timer
//#endif

#ifdef _WIN32
double CStopWatch::LIToSecs( LARGE_INTEGER & L) {
	return ((double)L.QuadPart /(double)frequency.QuadPart);
}
#else
// Useless dummy function to keep compatibility with the Windows implementation
double CStopWatch::LIToSecs( double & L) {
	return timer.stop-timer.start;
}
#endif

#ifdef _WIN32
CStopWatch::CStopWatch(){
	timer.start.QuadPart=0;
	timer.stop.QuadPart=0;	
	QueryPerformanceFrequency( &frequency );
}
#else
CStopWatch::CStopWatch(){
	timer.start = 0;
	timer.stop = 0;	
}
#endif

#ifdef _WIN32
void CStopWatch::startTimer( ) {
    QueryPerformanceCounter(&timer.start);
}
#else
void CStopWatch::startTimer( ) {
    timer.start = omp_get_wtime();
}
#endif

#ifdef _WIN32
void CStopWatch::stopTimer( ) {
    QueryPerformanceCounter(&timer.stop);
}
#else
void CStopWatch::stopTimer( ) {
    timer.stop = omp_get_wtime();
}
#endif

#ifdef _WIN32
double CStopWatch::getElapsedTime() {
	LARGE_INTEGER time;
	time.QuadPart = timer.stop.QuadPart - timer.start.QuadPart;
    return LIToSecs( time) ;
}
#else
double CStopWatch::getElapsedTime() {
    return timer.stop-timer.start;
}

#endif
