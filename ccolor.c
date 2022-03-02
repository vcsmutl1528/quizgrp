
#include <windows.h>
#include "ccolor.h"

static HANDLE hStdOut;

void cclrinit ()
{
	hStdOut = GetStdHandle (STD_OUTPUT_HANDLE);
}

unsigned char csetclr (unsigned char c)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	BOOL bRes;
	bRes = GetConsoleScreenBufferInfo (hStdOut, &csbi);
	SetConsoleTextAttribute (hStdOut, c);
	return bRes ? csbi.wAttributes : 0;
}

unsigned char cgetclr (void)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	BOOL bRes;
	bRes = GetConsoleScreenBufferInfo (hStdOut, &csbi);
	return bRes ? csbi.wAttributes : 0;
}

void csetclrv (unsigned char c)
{
	SetConsoleTextAttribute (hStdOut, c);
}
