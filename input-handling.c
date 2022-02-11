#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "input-handling.h"

/*
	Checks for the following within our input
	- If num_toks > 0
	- If line starts with '#'
	- Presence of '<'/'>' or '&'
	
	Returns -1 if line should not be ran (0 tokens, 
		or line starts with '#')
	Returns 0 otherwise.
	
	Sets input/output redirection flag (inrd/outrd) to its index if '<'/'>' is found (0 otherwise).
	
	Sets background flag (bg) to 1 if '&' is present at end of input (0 otherwise).
	When background flag is set, '&' is replaced with char*(0), and num_toks is decreased by 1.
	Note-- this means the first 2 arguments can be modified.
*/
int usrinputcheck(char** toks, size_t *num_toks, unsigned int *inrd, unsigned int *outrd, unsigned int *bg)
{
	*inrd = -1;
	*outrd = -1;
	*bg = 0;
	
	if(*num_toks == 0)
		return -1;

	else if(*num_toks > 0 && toks[0][0] == '#')
		return -1;

	else
	{
		for(size_t i = 0; i < *num_toks; i++)
		{
			if(strcmp(toks[i], "<") == 0) /* Sets input redirection flag */
			{
				*inrd = i;
			}
			else if(strcmp(toks[i], ">") == 0) /* Sets output redirection flag */
			{
				*outrd = i;
			}
		}
		
		if(strcmp(toks[*num_toks - 1], "&") == 0)
		{
			*bg = 1; /* Sets background flag */
			toks[*num_toks - 1] = (char*)(0);
			*num_toks = *num_toks - 1;
		}
		return 0;
	}
}

/*
	Finds any instances of "$$" within a given token (original)
	and replaces it with the given pid.
	
	Value of char* original is modified.
	Returns modified char.
*/
char* dollarztopid(char* original, const pid_t pid)
{
	if(original != NULL)
	{
		const int pidint = pid;
		char* new = malloc(sizeof(original));
		strcpy(new, original);
					
		for (int i = 0; i < strlen(original) - 1; i++)
		{
			if(original[i] == '$')
			{
				i++;
				if(original[i] == '$')
				{
					char* tmp1 = NULL;
					tmp1 = malloc(sizeof(original) * 2);
					char* tmp2 = NULL;
					tmp2 = malloc(sizeof(original));
					strncpy(tmp1, original, i - 1);
					strncpy(tmp2, &original[i+1], strlen(original) - (i));
					//strcat(tmp1, pidstr);
					//strcat(tmp1, tmp2);
					//strcpy(original, tmp1);
					new = realloc(new, sizeof(new) + 6);
					snprintf(new, 512, "%s%d%s", tmp1, pidint, tmp2);
					free(tmp1);
					free(tmp2);		
				}		
			}
		}
		return new;
	}

return original;
}

/*
	This function is called when input/output redirection is needed.
	The goal of this function is to properly redirect i/o before the
	child calls exec.
	
	Returns 0 if program is okay to exec.
	Returns -1 if program should NOT exec.
*/
int ioredirect(char** toks, int num_toks, const int inrd, const int outrd, const int bg)
{
	
	/* 	Checks if inrd flag is set
	When inrd flag is set, it is set to the index an input redirection call is at
	So, we must also check if the output redirection is not the last argument.
		
	If the flag is set, and there exists an argument after the flag (to use as the filename),
	we do input redirection!
	*/
	if(inrd != -1 && inrd < num_toks - 1)
	{
		char* filename = toks[inrd + 1];
		int fd = 0;
		if( (fd = open(filename, O_RDONLY)) != -1)
		{
			if( dup2(fd, STDIN_FILENO) == -1)
			{
				return -1;
			}
			
			toks[inrd + 1] = (char*)(0);
			toks[inrd] = (char*)(0);
			
			close(fd);
		}
		else
		{	
			err(errno, "Input redirection failed");
			return -1;
		}
	}
	
	/* 	Checks if outrd flag is set
		When outrd flag is set, it is set to the index an output redirection call is at
		So, we must also check if the output redirection is not the last argument.
		
		If the flag is set, and there exists an argument after the flag (to use as the filename),
		we do output redirection!
	*/
	if(outrd != -1 && outrd < num_toks - 1)
	{
		char* filename = toks[outrd + 1];
		int fd = 0;
		if( (fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG)) != -1)
		{
			dup2(fd, STDOUT_FILENO);
			//dup2(fd, STDERR_FILENO);
			
			toks[outrd + 1] = (char*)(0);
			toks[outrd] = (char*)(0);
			
			close(fd);
		}
	}
	
	/* Input redirection in background mode goes to /dev/null if not otherwise specified */
	if(bg == 1 && inrd == -1)
	{
		char* filename = "/dev/null";
		int fd = 0;
		if( (fd = open(filename, O_RDONLY)) != -1)
		{
			if( dup2(fd, STDIN_FILENO) == -1)
			{
				return -1;
			}
		
			close(fd);
		}
	}
	
	/* Output redirection in background mode goes to /dev/null if not otherwise specified */
	if(bg == 1 && outrd == -1)
	{
		char* filename = "/dev/null";
		int fd = 0;
		if( (fd = open(filename, O_WRONLY)) != -1)
		{
			if( dup2(fd, STDOUT_FILENO) == -1)
			{
				return -1;
			}
		
			close(fd);
		}
	}
	
return 0;
}