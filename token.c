/* Scans stdin for shell tokens and parses them. Supports quoting and
 * backslash escaping, according to POSIX specification.
 * 
 * ARGUMENTS:
 *  toks should be the address of a NULL-initialized char**. readTokens will
 *  allocate toks as a list of char*, and terminate the list in a NULL pointer,
 *  exactly like 'char *argv[]', so that toks can then be passed directly into 
 *  execv* family of functions
 *
 *  tok_num will be updated with the allocated size of toks, which may be larger
 *  than the number of tokens returned due to over-allocation. All pointers
 *  in toks are NULL if unset, so a simple for loop can free the whole structure:
 *    for (i=0; i<tok_num; ++i) free(toks[i]);
 *
 * Subsequent calls to readTokens can pass in previously allocated toks and 
 * tok_num variables to re-use memory, rather than re-allocating from scratch
 * each time a command is processed.
 * 
 * RETURN VALUE:
 *  The number of tokens is returned.
 *
 *
 * Example:
 *  Input on stdin: Hello world 'this is a quoted token'
 *  toks: { char* -> "Hello", 
 *          char* -> "world", 
 *          char* -> "this is a quoted token", 
 *          (char *) NULL, 
 *          ...,
 *          (char *) NULL
 *        }
 *  tok_num: sizeof(toks)
 *  return value: 3
 */
 
#include "token-parser.h"
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#ifndef BASE_TOK_SIZE
	#define BASE_TOK_SIZE 16
#endif

#ifndef BASE_TOK_NUM
	#define BASE_TOK_NUM 16
#endif

size_t readTokens(char ***toks, size_t *tok_num)
{
	/*	"Some magic to allocate the token list"
		-Prof Gambord
	*/
	if( *toks == NULL )
	{
		*tok_num = BASE_TOK_NUM;
		if( (*toks = malloc(*tok_num * sizeof(*toks) )) == NULL)
		{
			err(errno, "malloc failed");
		}
		for(size_t i = 0; i < *tok_num; ++i) (*toks)[i] = NULL;
	}
	
	size_t cur_tok = 0; // Our return value
	
	size_t tok_size = BASE_TOK_SIZE;
	if(((*toks)[cur_tok] = realloc((*toks)[cur_tok], tok_size * sizeof((*toks)[cur_tok]))) == NULL)
	{
		err(errno, "realloc failed");
	}
	
	size_t i = 0;
	const char quote = '\0'; //not sure if this can be a const or not.

	#if 0
	while(i >= tok_size) // This loops runs whenever we iterate past our currently allocated tok_size
	{
		tok_size *= 2;
		if(((*toks)[cur_tok] = realloc((*toks)[cur_tok], tok_size * sizeof((*toks)[cur_tok]))) == NULL)
		{
			err(errno, "realloc failed");
		}
	}
	#endif
			
	char* line = NULL; // Contains user's entire input
	size_t linelength = 0;
	int linesize = 0;
	linesize = getline(&line, &linelength, stdin); //Getting user input from stdin
	
	char* tmptok; // Temporary token
	char* saveptr = NULL;
	tmptok = strtok_r(line, " \n", &saveptr);
	while(tmptok != NULL)
	{	
		while(1)
		{
			// First, we must make sure we have allocated enough tokens
			if(cur_tok >= *tok_num - 1)
			{
				printf("need to do a realloc here....");
				*tok_num *= 2;
				//do a realloc here...
			}
				
			/* 	If we have alloc'd enough space within the token, 
				simply copy token over and break. */
			if( tok_size >= strlen(tmptok) ) 
			{
				//printf("Copying token over......\n");
				(*toks)[cur_tok] = tmptok;
				(*toks)[cur_tok + 1] = (char*)(0);
				//printf("Copy successful, (*toks)[cur_tok] == %s\n", *toks[cur_tok]);
				cur_tok++;
				tmptok = strtok_r(NULL, " \n", &saveptr);
				break;
			}	
			else // Otherwize, increase tok_size, realloc, and try again!
			{
				//printf("Increasing size of tok_size....\n");
				tok_size *= 2;
				if(((*toks)[cur_tok] = realloc((*toks)[cur_tok], tok_size * sizeof((*toks)[cur_tok]))) == NULL)
				{
					err(errno, "realloc failed");
				}
			}
		}
	}

//printf("hey im returning now... lol!! :D\n");
return cur_tok;
}