#ifndef TOKEN_PARSER_H__
#define TOKEN_PARSER_H__

#include <stddef.h>

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
size_t readTokens(char ***toks, size_t *tok_num);



#endif //TOKEN_PARSER_H__
