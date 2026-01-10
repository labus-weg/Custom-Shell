// The MIT License (MIT)
// 
// Copyright (c) 2023 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include<fcntl.h> 
#include <signal.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 128    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 10  // I changed it from 11 to 10 (original num of args supported by mav shell)
#define MAX_HISTORY 50      // req: "Your shell shall save the last 50 commands and command line parameters"
#define MAX_LENGTH 100      // supports a max of 100 chars per line of code


int main()
{
  
  /* signal handling with SIGINT and SIGTSTP
  our mav shell ignores these signals so only child processes respond to them
  */
  signal(SIGINT, SIG_IGN); /* ctrl + c => ignore & do nothing to stop operation */ 
  signal(SIGTSTP, SIG_IGN); /* same for ctrl + z */ 

  char * command_string = (char*) malloc( MAX_COMMAND_SIZE );
  char *hist[MAX_HISTORY] = {NULL};   // w/o null, free hist throws a warning about uninitialization. hist ptr just points to empty arr
  int sum_hist = 0; /* our history counter */     /* note for self: keep declarations clean and huddle them together if needed*/

  while( 1 )
  {
    // Print out the msh prompt
    printf ("msh> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (command_string, MAX_COMMAND_SIZE, stdin) );
    
    // handling blank inputs which are the command_string

    if(command_string[0] == '\n')
    {
      continue;   // doesn't affect shell at all
    }

    // saving to history array

    if (sum_hist == MAX_HISTORY)
    {
      free(hist[0]);
        
      for (int j = 1; j < MAX_HISTORY; j++)
      {
        hist[j-1] = hist[j];  // updates the hist arr so it stores the 2nd last string as its last before breaking out of loop
      }
      sum_hist--;   // history counter is decremented
    }
    hist[sum_hist++] = strdup(command_string);

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    for( int i = 0; i < MAX_NUM_ARGUMENTS; i++ )
    {
      token[i] = NULL;
    }

    // int strcmp(strtok)
    int token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *argument_ptr = NULL;                                         
                                                           
    char *working_string  = strdup( command_string );                

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;
   // Tokenize the input strings with whitespace used as the delimiter
    while ( ( (argument_ptr = strsep(&working_string, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( argument_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    if (token[0] == NULL )
    {
      free(head_ptr);
      continue;
    }

    // history: reimplemented in main func

    if (command_string[0] != '\n' && command_string[0] != '\t')   // history can only be implemented if prev cmds were NOT whitespaces
    {
      if (strcmp(token[0], "history") == 0)    // handling user input "history" as a cmd
      {
        for (int k = 0; k < sum_hist; k++)
        {
          printf("[%d] %s", k, hist[k]);
        }
        free(head_ptr);
        continue;
      }
    }

    // implementing pipe & redirection
    /* redirection merely changes file output position BEFORE exce fam mem is called */

    int pipe_ndx = -1;

    for (int j = 0; token[j] != NULL; j++)
    {
      if (strcmp(token[j], "|") == 0)
      {
        pipe_ndx = j;   /* updating index of the pipe */
      }
    }

    /* once this for loop ends, we know the position of the redirection in our input which has been tokenized */

    /* simple | implementation */
    if (pipe_ndx != -1)
    {
      int file_descriptor[2];   /* allocating a space of 2 fd */    /* file_des[0] = read end, file_des[1] = write end */
      pipe(file_descriptor);       /* currently, msh supports only 1 | */
      
      token[pipe_ndx] = NULL;   /* command is split */

      pid_t process1 = fork();
      if (process1 == 0)    /* forking the cpid*/
      {
        /* signal made to be default so that our program which is a child process responds to keyboard exit/interruption by ctrl+c or ctrl+z and not only via quit/exit */
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        dup2(file_descriptor[1], STDOUT_FILENO);    /* stdout_fileno writes to the output and represents file end of the | */
        close(file_descriptor[0]); 
        close(file_descriptor[1]);

        execvp(token[0], token);

        printf("%s: Command not found.\n", token[0]);

        exit(1);
      }
    
      pid_t process2 = fork();
      if (process2 == 0)    /* forking the cpid*/
      {
        dup2(file_descriptor[0], STDIN_FILENO); /* stdin_fileno reads from the input and represents beginning of the file for | */
        close(file_descriptor[0]); 
        close(file_descriptor[1]);

        execvp(token[pipe_ndx + 1], &token[pipe_ndx + 1]);

        printf("%s: Command not found.\n", token[pipe_ndx + 1]);

        exit(1);
      }

      close(file_descriptor[0]);
      close(file_descriptor[1]);

      waitpid(process1, NULL, 0);    
      waitpid(process2, NULL, 0);

      for (int i = 0; i < MAX_NUM_ARGUMENTS; i++)     /* this part is common to all cmds we're implementing */
      {                                               /* heap memory from string duplicate function strndup() is cleaned, so memory leak is taken care of */
        if (token[i] != NULL)
        {
          free(token[i]);
        }
      }
      free(head_ptr);
      continue;
    }

    /* redirection: input & output */

    int redir_input = -1;     /* structure is the same as pipe */
    int redir_output = -1;   
    
    for (int j = 0; token[j] != NULL; j++)
    {
      if (strcmp(token[j], "<") == 0)
      {
        redir_input = j;
      }
      else if (strcmp(token[j], ">") == 0)
      {
        redir_output = j;
      }
    }

    /* just input redirection */
    if (redir_input != -1)
    {
      pid_t pid = fork();
      if (pid == 0)
      {
        /* again, signal default then token handling */
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        int file_descriptor = open(token[redir_input + 1], O_RDONLY);

        if (file_descriptor == -1)
        {
          perror("Invalid open");
          exit(1);
        }

        dup2(file_descriptor, STDIN_FILENO);
        close(file_descriptor);

        token[redir_input] = NULL;    /* token cleared i.e., < & filename removed */

        /* only at this time can we execvp */
        execvp(token[0], token);
        printf("%s: Command not found.\n", token[0]);
        exit(1);
      }
      waitpid(pid, NULL, 0);   /* as usual, parent waits for forked child to finish */
      
      for (int i = 0; i < MAX_NUM_ARGUMENTS; i++)
      {
        if (token[i] != NULL)
          free(token[i]);
      }
      free(head_ptr);
      continue;
    }

    /* output redir is the exact same except > */
    if (redir_output != -1)
    {
      pid_t pid = fork();
      if (pid == 0)
      {
        /* again, signal default then token handling */
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        int file_descriptor = open(token[redir_output + 1], O_WRONLY);

        if (file_descriptor == -1)
        {
          perror("Invalid open");
          exit(1);
        }

        dup2(file_descriptor, STDOUT_FILENO);
        close(file_descriptor);

        token[redir_output] = NULL;    /* token cleared i.e., < & filename removed */

        /* only at this time can we execvp */
        execvp(token[0], token);
        printf("%s: Command not found.\n", token[0]);
        exit(1);
      }
      waitpid(pid, NULL, 0);   /* as usual, parent waits for forked child to finish */
      
      for (int i = 0; i < MAX_NUM_ARGUMENTS; i++)
      {
        if (token[i] != NULL)
          free(token[i]);
      }
      free(head_ptr);
      continue;
    }

    // implementing quit & exit () 
    if (strcmp(token[0], "exit") == 0)
    {
      exit(0);
    }
    else if (strcmp(token[0], "quit") == 0)
    {
      exit(0);
    }

    // cd implementation: cd ..  and cd <directory name>
    // only in parent process
    if (strcmp(token[0], "cd") == 0)
    {
      if (token[1] != NULL)
      {
        if (chdir(token[1]) != 0)
          perror("cd failed");
      }
      for (int i = 0; i < MAX_NUM_ARGUMENTS; i++)
      {
        if (token[i] != NULL)
        {
          free(token[i]);
        }
      }
      free(head_ptr);
      continue;
    }
    
    // ! implementation
    if (token[0][0] == '!')
    {
      int command_count = atoi(&token[0][1]);
      if (command_count >= 0 && command_count < sum_hist)   // user typed nothing or at max 50 commands (0 - 49)
      {
        strcpy(command_string, hist[command_count]);  // we copy the input string and reparse it like the first parsing of tokens

        for (int m = 0; m < MAX_NUM_ARGUMENTS; m++)
        {
          if (token[m] != NULL)
          {
            free(token[m]);
          }
        }
        free(head_ptr);
        
        // history needs retokenization too

        working_string  = strdup( command_string );     // same logic as before            
        head_ptr = working_string;
        token_count = 0; /* we reset this count as we're reparsing everything */
        // we are going to move the working_string pointer so
        // keep track of its original value so we can deallocate
        // the correct amount at the end
        // char *head_ptr = working_string; declared once already
        // Tokenize the input strings with whitespace used as the delimiter
        while ( ( (argument_ptr = strsep(&working_string, WHITESPACE ) ) != NULL) && 
                  (token_count<MAX_NUM_ARGUMENTS))
        {
          token[token_count] = strndup( argument_ptr, MAX_COMMAND_SIZE );
          if( strlen( token[token_count] ) == 0 )
          {
            token[token_count] = NULL;
          }
            token_count++;
        }

        if (token[0] == NULL )
        {
          free(head_ptr);
          continue;
        }
      }
    } 
      pid_t c_pid = fork();
      if (c_pid == 0) // inside child process
      {
        signal(SIGINT, SIG_DFL);    // w/o resetting the signals to default, fork() doesn't work properly (it copies the code with ignored signals instead)
        signal(SIGTSTP, SIG_DFL);

        execvp(token[0], &token[0]);

        // coommmand not found
        printf("%s: Command not found.\n", token[0]);
        fflush(NULL);
        exit(1);
      }
      else /* we're in the parent process */
      {
        // int status;
        waitpid(c_pid, NULL, 0);
        // fflush(NULL);
      }

        // Cleanup allocated memory
      for( int i = 0; i < MAX_NUM_ARGUMENTS; i++ )
      {
        if( token[i] != NULL )
        {
          free( token[i] );
        }
      }
      free( head_ptr );
  }
  free( command_string );
  exit(0); // e1234ca2-76f3-90d6-0703ac120004
} 
