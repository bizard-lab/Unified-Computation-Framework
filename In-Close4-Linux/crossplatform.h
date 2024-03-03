/* crossplatform.h

   Declarations for crossplatform code used in InClose4

   Written : March 2016

   Updated to compile on MacOS
   
   Last Update : June 2017

   Nuwan Kodagoda
   
*/

#ifndef CROSSPLATFORM_H
#define CROSSPLATFORM_H

#include <stdio.h>

// Linux Compilers
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#if defined(__GNUC__)
#include <mm_malloc.h>
#endif
// Common for all Linux/Unix/MacOS platforms
#define VECTOR_TYPE unsigned long long
#define VECTOR_1  1LL
#define ALIGNMENT __attribute__ ((aligned (32)))
#define ALIGNED_VECTOR_TYPE(n) VECTOR_TYPE n ALIGNMENT 

// Common Cross Platform Routines which are built in Windows

#include <string>
#include <iostream>
using namespace std;
#define ZeroMemory(p, sz) memset((p), 0, (sz))
int _kbhit(void);
void itoa(int n, char s[], int size);
void _itoa_s(int n, char s[], int size);
void _itoa_s(int n, char s[], int size, int radix);
void strcpy_s(char * destination, int buffer, const char * source);
int fopen_s(FILE **f, const char *name, const char *mode);
void reverse(char s[]);
#if defined(__linux__) || defined(__unix__)
// Not need for MacOS 
#if __GNUC__ < 6
string to_string(int i);
#endif
#endif

#ifdef __INTEL_COMPILER
#define DYN_ALIGNED_VECTOR_TYPE_ARR(n) (VECTOR_TYPE *) _mm_malloc(n*sizeof(VECTOR_TYPE),32)
#define DYN_ALIGNED_VECTOR_TYPE_PTR_ARR(n) (VECTOR_TYPE **) _mm_malloc(n*sizeof(VECTOR_TYPE *),32)
#define DYN_ALIGNED_DELETE_ARR(n) _mm_free(n)
#else
#ifdef _CRAYC 
#define DYN_ALIGNED_VECTOR_TYPE_ARR(n) new VECTOR_TYPE[n] ALIGNMENT
#define DYN_ALIGNED_VECTOR_TYPE_PTR_ARR(n) new VECTOR_TYPE*[n] ALIGNMENT
#define DYN_ALIGNED_DELETE_ARR(n) delete [] n
#else
#ifdef __GNUC__
#define DYN_ALIGNED_VECTOR_TYPE_ARR(n) (VECTOR_TYPE *) _mm_malloc(n*sizeof(VECTOR_TYPE),32)
#define DYN_ALIGNED_VECTOR_TYPE_PTR_ARR(n) (VECTOR_TYPE **) _mm_malloc(n*sizeof(VECTOR_TYPE *),32)
#define DYN_ALIGNED_DELETE_ARR(n) _mm_free(n)
#endif
#endif
#endif
#else
// Windows Intel and Microsoft Compilers

#include <conio.h>			//for _kbhit, windows can use this directly

#define ALIGNMENT __declspec(align(32)) 
#define VECTOR_TYPE unsigned __int64
#define VECTOR_1 1i64 
#define ALIGNED_VECTOR_TYPE(n) ALIGNMENT VECTOR_TYPE n 
#define DYN_ALIGNED_VECTOR_TYPE_ARR(n) new ALIGNMENT VECTOR_TYPE[n] 
#define DYN_ALIGNED_VECTOR_TYPE_PTR_ARR(n) new ALIGNMENT VECTOR_TYPE*[n]
#define DYN_ALIGNED_DELETE_ARR(n) delete [] n 
#endif

#endif  // CROSSPLATFORM_H
