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
 
#include token-parser.h
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>

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
	while(1)
	{
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
		ssize_t linesize = 0;
		linesize = getline(line, &linelength, stdin); //Getting user input from stdin
		
		char* tmptok; // Temporary token
		char* saveptr;
		while( tmptok = strtok_r(line, " ", &saveptr) != NULL )
		{
			while(1)
			{
				// First, we must make sure we have allocated enough tokens
				if(cur_tok > tok_num)
				{
					toknum *= 2;
					//do a realloc here...
				}
				
				/* 	If we have alloc'd enough space within the token, 
					simply copy token over and break. */
				if( sizeof((*toks)[cur_tok]) >= sizeof(tmptok) ) 
				{
					(*toks)[cur_tok] = tmptok;
					cur_tok++;
					break;
				}	
				else // Otherwize, increase tok_size, realloc, and try again!
				{
					tok_size *= 2;
					if(((*toks)[cur_tok] = realloc((*toks)[cur_tok], tok_size * sizeof((*toks)[cur_tok]))) == NULL)
					{
						err(errno, "realloc failed");
					}
				}
			}
		}
	}
	
return cur_tok;
}