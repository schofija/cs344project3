#include "token-parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h> //wait
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int usrinputcheck(char**, size_t*, unsigned int*, unsigned int*, unsigned int*);
void dollarztopid(char*, const pid_t);
int ioredirect(char**, int, const int, const int);

pid_t children[999];
int num_children = 0; // # of background child processes

void removechild(int index)
{
	for(int i = index; i < num_children; i++)
    {
        children[i] = children[i + 1];
    }
	
	for(int i=0; i<num_children; i++)
    {
            printf("%d\n", children[i]);
    }
}

void handlesig()
{
	for(int i = 0; i < num_children; i++)
	{
		int childstatus;
		if(waitpid(children[i], &childstatus, WNOHANG) != 0)
		{
			if(WIFEXITED(childstatus))
			{
				int status = WEXITSTATUS(childstatus);
				printf("\nbackground pid %d is done: terminated by signal %d\n:", children[i], childstatus);
			}
			else
				printf("\nbackground pid %d is done: terminated by signal %d\n:", children[i], WTERMSIG(childstatus));

			fflush(stdout);
			removechild(children[i]);
		}
	}
}

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
  
	const pid_t smallshpid = getpid();
	
	if(getpid() == smallshpid)
	{
		signal(SIGINT, SIG_IGN);
	}
	
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
    fprintf(stderr, ":");
	
    /* Call custom tokenizing function */
    num_toks = readTokens(&toks, &toks_size);
    size_t i;


	unsigned int inrd = -1; 		//Flag for input redirection ('<')
	unsigned int outrd = -1; 	//Flag for output redirection ('>').
	unsigned int bg = 0; 		//Flag for running proc in background ('&' at end of input line)

    if(usrinputcheck(toks, &num_toks, &inrd, &outrd, &bg) != -1)
    {
		//printf("inrd = %d, outrd = %d | bg = %d\n", inrd, outrd, bg);
		//fflush(stdout);
		
		/* 	Iterate through our tokens, calling dollarztopid()
			This results in all instances of "$$" being
			replaced with smallsh's pid...
		*/
		for (size_t i=0; i<num_toks; i++)
		{
			dollarztopid(toks[i], smallshpid);
		}
			
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
		#if 0
		else if (strcmp(toks[0], "echo")==0)
		{ /* echo command! -- shell internals */
			if (num_toks > 1) printf("%s", toks[1]);
			for (i=2; i<num_toks; ++i) printf(" %s", toks[i]);
			printf("\n");
		}
		#endif
		else if (strcmp(toks[0], "status")==0)
		{ /* echo command! -- shell internals */
			printf("%d\n", status);
			fflush(stdout);
			
		}
		else if (strcmp(toks[0], "exit")==0)
		{ /* echo command! -- shell internals */
			size_t i;
			for (i=0; i<num_toks; ++i)
				free(toks[i]);

			free(toks);
			free(cwd);
			signal(SIGQUIT, SIG_IGN);
			kill(0, SIGQUIT);
			exit(0);
		}
		else if(strcmp(toks[0], "#") == 0)
		{	
			/* This line is a comment. Do nothing! */	
		}
		else
		{ /* Default behavior: fork and exec */
			
			if(bg == 1)
			{
				signal(SIGCHLD,handlesig);
			}
			
			int childstatus = NULL;
			pid_t pid = fork();
			if (pid == 0)
			{ /* child */	

				/* Check for i/o redirection */
				if(ioredirect(toks, num_toks, inrd, outrd) != -1)
				{
					/* Child will respond to SIGINT */
					signal(SIGINT, SIG_DFL);
					
					execvp(toks[0], toks);
					err(errno, "failed to exec");
				}
				exit(1);
			}
			else if (pid == -1)
			{
				perror("fork() failed!");
				exit(1);
			}
			else
			{
				if(bg == 0)
				{
					pid = waitpid(pid, &childstatus, 0);
				}
				else if (bg != 0)
				{
					printf("background pid is %d\n", pid);
					children[num_children] = pid;
					num_children++;
					pid = waitpid(pid, &childstatus, WNOHANG);
				}

				if(WIFEXITED(childstatus) && bg == 0)
				{
					status = WEXITSTATUS(childstatus);
				}
			}
		}
    }
  } while(1);//while (num_toks > 0);
  

  size_t i;
  if(toks_size > 0)
  {
	  for (i=0; i<toks_size; ++i)
	  {
		free(toks[i]);
	  }
  }
  free(toks);
  free(cwd);

}

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
	No return value.
*/
void dollarztopid(char* original, const pid_t pid)
{
	const int pidint = pid;
	char pidstr[11] = {} ;
	sprintf(pidstr, "%d", pidint);

	for (int i = 0; i < strlen(original); i++)
	{
		if(original[i] == '$')
		{
			i++;
			if(original[i] == '$')
			{
				char* tmp1 = NULL;
				tmp1 = malloc(sizeof(original) + sizeof(pidstr));
				char* tmp2 = NULL;
				tmp2 = malloc(sizeof(original));
				strncpy(tmp1, original, i - 1);
				strncpy(tmp2, &original[i+1], strlen(original) - (i));
				strcat(tmp1, pidstr);
				strcat(tmp1, tmp2);
				//char * tmp3 = realloc(original, sizeof(tmp1));

				strcpy(original, tmp1);
				free(tmp1);
				free(tmp2);			
			}		
		}
	}
}

/*
	This function is called when input/output redirection is needed.
	The goal of this function is to properly redirect i/o before the
	child calls exec.
	
	Returns 0 if program is okay to exec.
	Returns -1 if program should NOT exec.
*/
int ioredirect(char** toks, int num_toks, const int inrd, const int outrd)
{
	
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
			return -1;
		}
	}
	
	if(outrd != -1 && outrd < num_toks - 1)
	{
		char* filename = toks[outrd + 1];
		//printf("hey\n");
		//printf( "FILENAME: %s", filename);
		fflush(stdout);
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
return 0;
}
