/* -----------------------------------------------------------------------------
FILE: shell.c

NAME:

DESCRIPTION: A SHELL SKELETON
-------------------------------------------------------------------------------*/


#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"   // local file include declarations for parse-related structs


enum BUILTIN_COMMANDS { NO_SUCH_BUILTIN=0, EXIT, JOBS };
char gosh[1024] = "gosh@";
char cwd[1024];
char* ending = "< ";


/* -----------------------------------------------------------------------------
FUNCTION: buildPrompt()
DESCRIPTION:
-------------------------------------------------------------------------------*/
char * buildPrompt()
{

    return gosh;
}


/* -----------------------------------------------------------------------------
FUNCTION: isBuild()
DESCRIPTION:
-------------------------------------------------------------------------------*/
int isBuiltInCommand( char * cmd )
{
  if( strncmp(cmd, "exit", strlen( "exit" ) ) == 0 )
	{
	return EXIT;
  	}

  return NO_SUCH_BUILTIN;
}

/* -----------------------------------------------------------------------------
FUNCTION: isBuild()
DESCRIPTION:
-------------------------------------------------------------------------------*/
void execute_pipeline(char *commands[], char **commands_args[], int number_commands,
int input_fd, char* input, int output_fd, char* output) {
    int pipe_fd[number_commands - 1][2];
    pid_t pid[number_commands];

    // checking if the pipe works
    for (int i = 0; i < number_commands - 1; i++) {
        if (pipe(pipe_fd[i]) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

    }

    // creating a child each time and checking if it works
    for (int i = 0; i < number_commands; i++) {
        pid_t new_pid = fork();
        if (new_pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
            // if it works, then it will either create a dup2 for pipe's read end onto STDIN_FILENO
            // and check that or it will create a dup2 for pipe's write end onto STDOUT_FILENO
        } else if (new_pid == 0) {
            if (i > 0) {
                if (dup2(pipe_fd[i - 1][0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }


             }

            if (i < number_commands - 1) {
                if (dup2(pipe_fd[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

            }
            // here it will check for input and output redirection
            if (input_fd != 0) {
                int in = open(input, O_RDONLY);
                 if (in < 0) {
                     perror("open");
                     exit(EXIT_FAILURE);
                 }
                 if (dup2(in, STDIN_FILENO) < 0) {
                     perror("dup2");
                     exit(EXIT_FAILURE);
                 }

                 close(in);

            }

            if (output_fd != 0) {
                int out = open(output, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                   if (out < 0) {
                       perror("open");
                       exit(EXIT_FAILURE);
                   }
                   if (dup2(out, STDOUT_FILENO) < 0) {
                       perror("dup2");
                       exit(EXIT_FAILURE);
                   }

                   close(out);
            }

            // closing the pipes
            for (int j = 0; j < number_commands - 1; j++) {
                if (i != j) {
                    close(pipe_fd[j][0]);
                    close(pipe_fd[j][1]);
                }
            }

            // executing the commands and checking if it works
            execvp(commands[i], commands_args[i]);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else {
             pid[i] = new_pid;
             // closing the pipes if they have not been closed yet
             if (i > 0) {
                 close(pipe_fd[i - 1][0]);
                 close(pipe_fd[i - 1][1]);
             }

        }
    }

    // wait() for each child
    for (int i = 0; i < number_commands; i++) {
        if (waitpid(pid[i], NULL, 0) < 0) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
    }

}


/* -----------------------------------------------------------------------------
FUNCTION: main()
DESCRIPTION:
-------------------------------------------------------------------------------*/
int main( int argc, char **argv )
{

    char * cmdLine;
    parseInfo *info; 		// info stores all the information returned by parser.
    struct commandType *com; 	// com stores command name and Arg list for one command.
    int count = 0;
    fprintf( stdout, "This is the SHELL version 0.1\n" ) ;
    strcat(gosh, getenv("PWD"));
    strcat(gosh, "< ");
    int input_fd = 0;
    int output_fd = 0;



    while(1)
    {


    	cmdLine = readline( buildPrompt() ) ;
    	if( cmdLine == NULL )
        {
      		fprintf(stderr, "Unable to read command\n");
      		continue;
    	}


    	// calls the parser

    	info = parse( cmdLine );
        if( info == NULL )
        {
      		free(cmdLine);
      		continue;
    	}

    	//com contains the info. of the command before the first "|"
    	com = &info->CommArray[0];
    	if( (com == NULL)  || (com->command == NULL))
    	{
      		free_info(info);
      		free(cmdLine);
      		continue;
    	}

    	//com->command tells the command name of com
    	if( isBuiltInCommand( com->command ) == EXIT )
        {
      		exit(1);
    	}

        // checking if command line contains input, output, or pipes
        for (int i = 0; i < strlen(cmdLine); i++) {
            if (cmdLine[i] == '<' || cmdLine[i] == '>') {
                count = 1;
            } else if (cmdLine[i] == '|') {
                count = 2;
            }

        }
        // if it has pipes, it will execute the execute_pipline method
        if (count == 2) {
            int size = info->pipeNum+1;
            char* commands[size];
            char** pipeline_commands[size];
            for (int i = 0; i < size; i++) {
                commands[i] = *info->CommArray[i].VarList;
                pipeline_commands[i] = info->CommArray[i].VarList;
            }

            execute_pipeline(commands, pipeline_commands, size, info->boolInfile, info->inFile,
            info->boolOutfile, info->outFile);
            // if it has input or output, it will redirect based on that
        } else if (count == 1) {
            pid_t pid = fork();
            if (pid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                if (info -> boolInfile != 0) {
                    // opening file and checking
                    input_fd = open(info->inFile, O_RDONLY);
                    if (input_fd < 0) {
                        perror("input");
                        exit(1);
                    }
                    // dup2 for the file
                    if (dup2(input_fd, STDIN_FILENO) < 0) {
                        perror("dup");
                        exit(1);
                    }
                    // closing the file and then executing
                    close(input_fd);
                    execvp(com -> command, com -> VarList);
                    perror("execvp");
                } else if (info -> boolOutfile != 0) {
                    // creating the file and then checking
                    output_fd = open(info->outFile, O_WRONLY | O_CREAT |
                    O_TRUNC, S_IRUSR | S_IWUSR);
                    if (output_fd < 0) {
                        perror("output");
                        exit(1);
                    }
                    // dup2 for the file
                    if (dup2(output_fd, STDOUT_FILENO) < 0) {
                        perror("out");
                        exit(1);
                    }
                    // closing the file and executing
                    close(output_fd);
                    execvp(com -> command, com -> VarList);
                    perror("execvp");
                } else if (info -> boolInfile != 0 && info -> boolOutfile != 0) {
                    // opening file and checking
                    input_fd = open(info->inFile, O_RDONLY);
                    if (input_fd < 0) {
                        perror("input");
                        exit(1);
                    }
                    // dup2 for the file
                      if (dup2(input_fd, STDIN_FILENO) < 0) {
                          perror("dup");
                          exit(1);
                      }
                      // closing
                      close(input_fd);
                      // creating the file and checking
                      output_fd = open(info->outFile, O_WRONLY |
                      O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                      if (output_fd < 0) {
                          perror("output");
                          exit(1);
                      }
                      // dup2 for the file
                      if (dup2(output_fd, STDOUT_FILENO) < 0) {
                          perror("out");
                          exit(1);
                      }
                      // closing and then executing
                      close(output_fd);
                      execvp(com -> command, com -> VarList);
                      perror("execvp");
                }
            } else if (pid > 0) {
                // waiting for child
                int status;
                waitpid(pid, &status, 0);
            }
        } else if (count == 0) {
            // executes pwd command by using fork() and waitpid()
            if (cmdLine[0] == 'p' && cmdLine[1] == 'w' && cmdLine[2] == 'd') {
                pid_t pid = fork();
                if (pid == 0) {
                    char *args[] = {"pwd", NULL};
                    execvp(args[0], args);
                    perror("execvp");
                    exit(EXIT_FAILURE);
                } else {
                    int status;
                    waitpid(pid, &status, 0);
                }
                // executes help command and provides print statements
            } else if (cmdLine[0] == 'h' && cmdLine[1] == 'e'
            && cmdLine[2] == 'l' && cmdLine[3] == 'p') {
                printf("Here are the commands provided for the commandline:\n");
                printf("cd [optional-argument]\npwd\nls -l [optional-arg]\ngrep -h [arg]\n");
                continue;
                // executes cd command by using chdir()
            } else if (cmdLine[0] == 'c' && cmdLine[1] == 'd') {
                if (cmdLine[3] == '~') {
                    char* home = getenv("HOME");
                    strcat(home, &cmdLine[4]);
                    if (chdir(home) == -1) {
                        perror("chdir");
                        exit(0);
                    }

                } else {
                    if (chdir(&cmdLine[3]) == -1) {
                        perror("chdir");
                        exit(0);
                    }
                }
            gosh[0] = '\0';
            strcat(gosh, "gosh@");
            strcat(gosh, &cmdLine[3]);
            strcat(gosh, "< ");
            continue;
                // executes exit command
            } else if (cmdLine[0] == 'e') {
                exit(EXIT_FAILURE);
                // executes other commands by using fork() and waitpid()
            } else {
                pid_t pid = fork();
                if (pid == 0) {

                    execvp(com -> command, com -> VarList);
                    perror("execvp");
                    exit(EXIT_FAILURE);
                } else {
                    int status;
                    waitpid(pid, &status, 0);
                }

            }


        }
    free_info(info);

    free(cmdLine);


    }/* while(1) */

} // main
