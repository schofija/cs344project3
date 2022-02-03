#include "token-parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h> //wait


int
main(int argc, char *argv[])
{
  char **toks = NULL;
  size_t toks_size;
  size_t num_toks;
  char *cwd = NULL;
  char *tmp_cwd;
  char *login;
  char hostname[HOST_NAME_MAX+1];
  int status = NULL;
  
  do
  {
    { /* Increase cwd buffer size until getcwd is successful */
      size_t len = 0;
      while (1) 
      {
        len += 16;
        cwd = realloc(cwd, len * sizeof *cwd);
        if (getcwd(cwd, len) == NULL)
        {
          if (errno == ERANGE) continue;
          err(errno, "getcwd failed");
        }
        else break;
      }
    }
    char *homedir = getenv("HOME");
    
    { /* Replace home directory prefix with ~ */
      size_t len = strlen(homedir);
      tmp_cwd = cwd;
      if(strncmp(tmp_cwd, homedir, len) == 0)
      {
        tmp_cwd += len-1;
        *tmp_cwd = '~';
      }
    }

    login = getlogin();
    gethostname(hostname, sizeof(hostname));

    /* Print out a simple prompt */
    fprintf(stderr, ": ");
    
    /* Call custom tokenizing function */
    num_toks = readTokens(&toks, &toks_size);
    size_t i;

    if (num_toks > 0)
    {
		if (strcmp(toks[0], "cd")==0)
		{ /* cd command -- shell internals */
			if (num_toks == 1) 
			{
			  if(chdir(homedir) == -1)
			  {
				perror("Failed to change directory");
			  }
			}
			else if (chdir(toks[1]) == -1)
			{
			  perror("Failed to change directory");
			}
		}
		else if (strcmp(toks[0], "echo")==0)
		{ /* echo command! -- shell internals */
			if (num_toks > 1) printf("%s", toks[1]);
			for (i=2; i<num_toks; ++i) printf(" %s", toks[i]);
			printf("\n");
		}
		else if (strcmp(toks[0], "status")==0)
		{ /* echo command! -- shell internals */
			printf("%d", status);
			fflush(stdout);
			
		}
		else if (strcmp(toks[0], "exit")==0)
		{ /* echo command! -- shell internals */
			size_t i;
			for (i=0; i<toks_size; ++i)
				free(toks[i]);

			free(toks);
			free(cwd);
			exit(0);
		}
		else if(strcmp(toks[0], "#") == 0)
		{	/* This line is a comment. Do nothing! */
			
		}
		else
		{ /* Default behavior: fork and exec */
			int childstatus = NULL;
			pid_t pid = fork();
			if (pid == 0)
			{ /* child */
				execvp(toks[0], toks);
				err(errno, "failed to exec");
			}
			else if (pid == -1)
			{
				perror("fork() failed!");
				exit(1);
			}
			else
			{
				pid = waitpid(pid, &childstatus, 0);
				if(WIFEXITED(childstatus))
					status = WEXITSTATUS(childstatus);
			}
        //childstatus = wait(NULL);
		}
    }
  } while(1);//while (num_toks > 0);
  

  size_t i;
  for (i=0; i<toks_size; ++i)
  {
    free(toks[i]);
  }
  free(toks);
  free(cwd);

}
