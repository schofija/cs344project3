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
static volatile sig_atomic_t children[MAX_BGPROCS]; /* Array of background processes */
static volatile sig_atomic_t num_children = 0; /* # of background child processes */

static volatile sig_atomic_t bgflag = 0; /* Flag to determine if we should print. Also stores how many BG procs finished */
static volatile sig_atomic_t bgpid[MAX_BGPROCS]; /* Stores all finished background proc pids that need to be printed */
static volatile sig_atomic_t bgstatus[MAX_BGPROCS]; /* Stores all exit status's that need to be printed */

/* Background mode flags */
static volatile sig_atomic_t fg_onlymode = 0;
static volatile sig_atomic_t fg_onlymodeflag = 0;

/* Foreground process flags */
static volatile sig_atomic_t sigintflag = 0; /* Set to 1 if SIGINT has been sent */
static volatile sig_atomic_t num_fgchildren = 0; /* Tracks if foreground children exist */

void handlesigchld();
void handlesigtstp();
void handlesigint();

int
main(int argc, char *argv[])
{
  char **toks = NULL;
  size_t toks_size;
  size_t num_toks;
  int status = NULL;
  
	const pid_t smallshpid = getpid();
	
	if(getpid() == smallshpid)
	{
		signal(SIGINT, handlesigint);
		signal(SIGTSTP, handlesigtstp);
	}
	
  do
  {
    char *homedir = getenv("HOME");
    
	if(sigintflag == 1) /* If sigintflag is set, print a message*/
	{
		status = 2; /* Manually setting status to 2 (SIGINT) */
		sigintflag = 0;
		printf("\nTerminated by signal 2\n");
	}
	
	if(bgflag > 0) /* If bgflag is set, print out all background processes that have ended */
	{
		for(int i = 0; i<bgflag; i++)
		{
			printf("background pid %d is done: terminated by signal %d\n", bgpid[i], bgstatus[i]);
			fflush(stdout);
		}
		bgflag = 0;
	}
	
	if(fg_onlymodeflag == 1) /* If flag is set, print status of foreground-only mode */
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
			
	/* Print out a simple prompt */
    fprintf(stderr, ":");
	
    /* Call custom tokenizing function */
    num_toks = readTokens(&toks, &toks_size, smallshpid);
    size_t i;

	unsigned int inrd = -1; 	//Flag for input redirection ('<')
	unsigned int outrd = -1; 	//Flag for output redirection ('>').
	unsigned int bg = 0; 		//Flag for running proc in background ('&' at end of input line)

    if(usrinputcheck(toks, &num_toks, &inrd, &outrd, &bg) != -1) /* Determine if user input is a blankline (-1), or if it should be ran */
    {	
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
			{ /* If command is to be ran in background mode, set signal handler for SIGCHLD*/
				signal(SIGCHLD,handlesigchld);
			}
			
			int childstatus = NULL;
			pid_t pid = fork();
			if (pid == 0)
			{ /* child */	

				/* Check for i/o redirection */
				if(ioredirect(toks, num_toks, inrd, outrd, bg) != -1)
				{
					/* All children should ignore SIGTSTP */
					signal(SIGTSTP, SIG_IGN);
					
					if(bg == 0)
					{			
						signal(SIGINT, SIG_DFL); /* Child procs in foreground mode respond to SIGINT */
					}
					
					else if (bg == 1)
						signal(SIGINT, SIG_IGN); /* Child procs in background mode IGNORE SIGINT */
	
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
					children[num_children] = pid; 	/* Update array with new background child process */
					num_children = num_children + 1;
					pid = waitpid(pid, &childstatus, WNOHANG);
				}
				else /* Run proc in foreground */
				{
					num_fgchildren = num_fgchildren + 1;
					pid = waitpid(pid, &childstatus, 0);
				}

				if(WIFEXITED(childstatus) && bg == 0)
				{
					num_fgchildren = 0;
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
}

void handlesigchld() /* Handles background processes that have ended and sent a SIGCHLD */
{
	for(int i = 0; i < num_children; i++)
	{
		int childstatus;
		if(waitpid(children[i], &childstatus, WNOHANG) != 0 && children[i] != 0)
		{
			if(WIFEXITED(childstatus))
			{
				int status = WEXITSTATUS(childstatus);
				bgpid[bgflag] = children[i];
				bgstatus[bgflag] = status;
				bgflag = bgflag + 1; //increment bgflag
			}
			else
			{
				bgpid[bgflag] = children[i]; //set bgpid[bgflag] to pid
				bgstatus[bgflag] = WTERMSIG(childstatus); //set bgstatus[bgflag] to exit status
				bgflag = bgflag + 1; //increment bgflag
			}

			if(num_children == 1)
			{
				children[0] = 0;
				num_children = 0;
			}
			else
			{
				for(int j = i; j < num_children; j++)
				{
					children[j] = children[j + 1];
				}
				num_children = num_children - 1;
			}
		break;
		}
	}
}

void handlesigtstp() /* Toggles global flags. These flags are used to determine if/what to print */
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

void handlesigint() /* Sets sigintflag if there were foreground children killed by the SIGINT signal */
{
	if(num_fgchildren > 0)
	{
		sigintflag = 1;
	}
}
