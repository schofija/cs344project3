#ifndef INPUT_HANDLING_H__
#define INPUT_HANDLING_H__

#include <stddef.h>
#include <unistd.h>

/*
	This header file contains declarations for all the functions
	used within smallsh that check (and sometimes modify) the user's input.
	
	usrinputcheck() -- 	determines whether user input should be executed (checks for comment/blankline)
						sets input/output redirection flag/background flag if needed
	
	dollarztopid() -- 	handles "$$" -> pid transformation
	
	ioredirect() --	 handles input/output redirection
*/

int usrinputcheck(char**, size_t*, unsigned int*, unsigned int*, unsigned int*);
char* dollarztopid(char*, const pid_t);
int ioredirect(char**, int, const int, const int, const int);

#endif //INPUT_HANDLING_H__
