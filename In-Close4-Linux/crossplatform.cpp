#include "crossplatform.h"

/* crossplatform.cpp

Declarations for crossplatform code used in In-Close4

Last Change : March 2016

Updated to compile on MacOS

Last Update : June 2017

Nuwan Kodagoda

*/

// For windows conio.h _kbhit()

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)

// Code there only for Linux/MacOS Platforms

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>


int _kbhit(void)
{
	struct termios oldt, newt;
	int ch;
	int oldf;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

	ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if (ch != EOF)
	{
		ungetc(ch, stdin);
		return 1;
	}

	return 0;
}

/* itoa:  convert n to characters in s */
void itoa(int n, char s[], int size)
{
	int i, sign;

	if ((sign = n) < 0)  /* record sign */
		n = -n;          /* make n positive */
	i = 0;
	do {       /* generate digits in reverse order */
		s[i++] = n % 10 + '0';   /* get next digit */
	} while ((n /= 10) > 0);     /* delete it */
	if (sign < 0)
		s[i++] = '-';
	s[i] = '\0';
	reverse(s);
}

void _itoa_s(int n, char s[], int size) {
	itoa(n, s, size);
}
void _itoa_s(int n, char s[], int size, int radix) {
	itoa(n, s, size);
}
void strcpy_s(char * destination,int buffer, const char * source) {
	strcpy(destination, source);
} 

int fopen_s(FILE **f, const char *name, const char *mode) {
	int ret = 0;
	assert(f);
	*f = fopen(name, mode);
	/* Can't be sure about 1-to-1 mapping of errno and MS' errno_t */
	if (!*f)
		ret = -99;
	return ret;
}

/* reverse:  reverse string s in place */
void reverse(char s[])
{
	int i, j;
	char c;

	for (i = 0, j = strlen(s) - 1; i<j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
}

string to_string(int i) {
	char intStr[512];
	_itoa_s(i, intStr, 10);
	return string(intStr);
}

#endif
