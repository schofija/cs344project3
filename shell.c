#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "token-parser.h"
#include "input-handling.h"

#ifndef MAX_BGPROCS
	#define MAX_BGPROCS 999
#endif

/* These global variables are used to store the pid values of
	all background child processes currently running.
*/
static volatile sig_atomic_t children[MAX_BGPROCS];
static volatile sig_atomic_t num_children = 0; // # of background child processes

/* Background mode flags */
static volatile sig_atomic_t fg_onlymode = 0;
static volatile sig_atomic_t fg_onlymodeflag = 0;

void handlesigchld();
void handlesigtstp();

int
main(int argc, char *argv[])
{
  char **toks = NULL;
  size_t toks_size;
  size_t num_toks;
  //char *cwd = NULL;
  //char *tmp_cwd;
  //char *login;
  //char hostname[HOST_NAME_MAX+1];
  int status = NULL;
  
	const pid_t smallshpid = getpid();
	
	if(getpid() == smallshpid)
	{
		signal(SIGINT, SIG_IGN);
		signal(SIGTSTP, handlesigtstp);
	}
	
  do
  {
	  
	#if 0
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
	#endif
    char *homedir = getenv("HOME");
    
	#if 0
    { /* Replace home directory prefix with ~ */
      size_t len = strlen(homedir);
      tmp_cwd = cwd;
      if(strncmp(tmp_cwd, homedir, len) == 0)
      {
        tmp_cwd += len-1;
        *tmp_cwd = '~';
      }
    }
	#endif

    //login = getlogin();
   //gethostname(hostname, sizeof(hostname));
	
    /* Print out a simple prompt */
	if(fg_onlymodeflag == 1)
	{
		if(fg_onlymode == 0)
		{
			printf("Exiting foreground-only mode\n");
		}
		else
		{
			printf("Entering foreground-only mode (& is now ignored)\n");
		}
		fg_onlymodeflag = 0;
		fflush(stdout);
	}
			
    fprintf(stderr, ":");
	
    /* Call custom tokenizing function */
    num_toks = readTokens(&toks, &toks_size);
    size_t i;


	unsigned int inrd = -1; 		//Flag for input redirection ('<')
	unsigned int outrd = -1; 	//Flag for output redirection ('>').
	unsigned int bg = 0; 		//Flag for running proc in background ('&' at end of input line)

    if(usrinputcheck(toks, &num_toks, &inrd, &outrd, &bg) != -1)
    {
		for (size_t i=0; i<num_toks; i++)
		{ 	/* Replace all instances of "$$" with pid */
			toks[i] = dollarztopid(toks[i], smallshpid);
		}
			
		if (strcmp(toks[0], "cd")==0)
		{ /* cd command -- built in */
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
		{ /* status command! -- built in */
			printf("exit status %d\n", status);
			fflush(stdout);
		}
		else if (strcmp(toks[0], "exit")==0)
		{ /* exit command! -- built in */
			size_t i;
			for (i=0; i<num_toks; ++i)
				free(toks[i]);

			free(toks);
			//free(cwd);
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
				signal(SIGCHLD,handlesigchld);
			}
			
			int childstatus = NULL;
			pid_t pid = fork();
			if (pid == 0)
			{ /* child */	

				/* Check for i/o redirection */
				if(ioredirect(toks, num_toks, inrd, outrd) != -1)
				{
					/* All children should ignore SIGTSTP */
					signal(SIGTSTP, SIG_IGN);
					
					/* Child will respond to SIGINT */
					if(bg == 0)
						signal(SIGINT, SIG_DFL);
					
					else if (bg == 1)
						signal(SIGINT, SIG_IGN);
	
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
				if (bg == 1 && fg_onlymode == 0) /* Run proc in background */
				{
					printf("background pid is %d\n", pid);
					children[num_children] = pid;
					num_children++;
					pid = waitpid(pid, &childstatus, WNOHANG);
				}
				else /* Run proc in foreground */
				{
					pid = waitpid(pid, &childstatus, 0);
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
  //free(cwd);

}

void handlesigchld()
{
	for(int i = 0; i < num_children; i++)
	{
		int childstatus;
		if(waitpid(children[i], &childstatus, WNOHANG) != 0 && children[i] != 0)
		{
			if(WIFEXITED(childstatus))
			{
				int status = WEXITSTATUS(childstatus);
				printf("\nbackground pid %d is done: terminated by signal %d\n:", children[i], status);
				//write(STDOUT_FILENO, "\nbackground pid %d is done: terminated by signal", 60);
				//write(STDOUT_FILENO, status, 6);
			}
			else
			{
				printf("\nbackground pid %d is done: terminated by signal %d\n", children[i], WTERMSIG(childstatus));
				//write(STDOUT_FILENO, "\ntesting1\n", 10);

			}

			fflush(stdout);
			for(int j = i; j < num_children; j++)
			{
				children[j] = children[j + 1];
			}
		break;
		}
	}
}

void handlesigtstp()
{
	fg_onlymodeflag = 1;
	
	if(fg_onlymode == 0)
	{
		fg_onlymode = 1;
	}
	else if(fg_onlymode == 1)
	{
		fg_onlymode = 0;
	}	
}
