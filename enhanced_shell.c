#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_CHAR 50
#define MAX_TOK 50
#define MAX_CMD 50
#define MAX_HIST 6

int main() {
  
  char history[MAX_HIST][MAX_CHAR];
  int histCount = 0;

  while (1) {
    char user_input[MAX_CHAR];
    char* argv[MAX_TOK]; // pointer to array of char ptrs
    char* tok;
    int tokCount = 0;
    int cmdCount = 1;
    int cmdStart[MAX_CMD]; // store index of start of each command
    int afterRedir = 0; // determines if token is currently after a redirection cmd
    
    // pointer to array of each redirection file/corresponding forced variant array
    char* stdInFiles[MAX_CMD] = {NULL};
    char* stdOutFiles[MAX_CMD] = {NULL};
    int forcedOut[MAX_CMD] = {0};

    // prompt and store user input, exit on error
    printf("Enter a command: ");
    if (fgets(user_input, sizeof(user_input), stdin) == NULL) {
      printf("\nEOF or an error has occured reading from stdin. Exiting...\n");
      break;
    }
  
    // redo func.
    if (user_input[0] == '!') {
      int num = atoi(user_input + 1); // number after first char (!)
      
      if (num < 1) {
        printf("Cannot redo numbers less than 1.\n");
        continue;
      } else if (num > histCount) {
        printf("Cannot redo numbers greater than 6.\n");
        continue;
      }
     
      // copy string in history into user_input 
      strncpy(user_input, history[histCount - num], MAX_CHAR - 1); // MAX_CHAR - 1 for null term.
      user_input[MAX_CHAR - 1] = '\0';
      
      printf("%s\n", user_input);
    }
    
    // must copy string - strtok modifies it.
    char historyLine[MAX_CHAR];
    strncpy(historyLine, user_input, MAX_CHAR);
    historyLine[strcspn(historyLine, "\n")] = '\0'; // remove trailing newline
    
    // parse
    cmdStart[0] = 0;
    tok = strtok(user_input, " \n");
    while (tok != NULL && tokCount < MAX_TOK - 1) { // leave room for NULL (just in case)
      if (strcmp(tok, "|") == 0) {
    
        // append NULL to current argv array when "|" seen
        argv[tokCount] = NULL;
        tokCount++;
        
        // store starting index of command
        cmdStart[cmdCount] = tokCount;
        cmdCount++;
        afterRedir = 0;
      } else if (strcmp(tok, ">") == 0 || strcmp(tok, ">!") == 0) { // stdout func. (normal/forced variant)
        int forced = (strcmp(tok, ">!") == 0); // track variant, 0 = force var., any other int. = normal var.
        //printf("forced: %d\n", forced);
        tok = strtok(NULL, " \n"); // end before filename
        if (tok == NULL) {
          perror("No file name input after '>'.\n");
          exit(3);
        }
        stdOutFiles[cmdCount - 1] = tok;
        forcedOut[cmdCount - 1] = forced;
        afterRedir = 1;
      } else if (strcmp(tok, "<") == 0) { // stdin func.
        tok = strtok(NULL, " \n");
        if (tok == NULL) {
          perror("No file name input after '<'. \n");
          exit(4);
        }
        stdInFiles[cmdCount - 1] = tok;
        afterRedir = 1;
      } else {  
        if (afterRedir == 0) { // add tokens if not after a redirection
          argv[tokCount] = tok;
          tokCount++;
        }
      }
      tok = strtok(NULL, " \n"); // continue after each command "end" within the user_input string
    }
    
    // notify and continue if no command entered
    if (tokCount == 0) {
      printf("No command entered.\n");
      continue;
    }
    
    // check first arg. for "quit" flag
    if (strcmp(argv[0], "quit") == 0) {
      printf("Quitting...\n");
      exit(93);
    }
    
    // check for "history" command
    if (cmdCount == 1 && strcmp(argv[0], "history") == 0) {
      for (int c = histCount, i = 0; c <= histCount && i < histCount; c--, i++) {
        printf("%d %s\n", c, history[i]);
      }
      continue;
    }
    
    // append NULL to end of argv
    argv[tokCount] = NULL;
    
    // create pipes for each command (besides last)
    int fd[MAX_CMD - 1][2];
    for (int p = 0; p < cmdCount - 1; p++) {
      if (pipe(fd[p]) < 0) {
        perror("Pipe failed.");
        exit(1);
      }
    } 
    
    // each command, a child process is created
    for (int i = 0; i < cmdCount; i++) {
      pid_t child = fork();
      
      if (child < 0) {
        perror("Fork failed.");
        exit(2);
      }

      if (child == 0) {
         
        // read from previous pipe, not initial command (which reads from stdin)
        if (i > 0) {
          dup2(fd[i-1][0], 0);
        }

        // write into next pipe, not last command (which doesn't need pipe)
        if (i < cmdCount - 1) {
          dup2(fd[i][1], 1);
        }

        // close ends of all pipes
        for (int e = 0; e < cmdCount - 1; e++) {
          close(fd[e][0]);
          close(fd[e][1]);
        }
       
        // stdout func. 
        if (stdOutFiles[i] != NULL) {
          if (forcedOut[i] != 1) {
            int fd_rec = open(stdOutFiles[i], O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR); // O_CREAT and O_EXCL combined check for existing file
            if (fd_rec < 0) {
              perror("File exists. Cannot overwrite.\n");
              exit(10);
            }
          }
          int fd_rec = open(stdOutFiles[i], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); // allow overwrite
          dup2(fd_rec, 1);
          close(fd_rec);
        }

        // stdin func.
        if (stdInFiles[i] != NULL) {
          int fd_in = open(stdInFiles[i], O_RDONLY, 0);
          if (fd_in < 0) {
            perror("File does not exist. Cannot read.\n");
            exit(6);
          }
          dup2(fd_in, 0);
          close(fd_in);
        }
        
        // run command w/ given args
        if (execvp(argv[cmdStart[i]], &argv[cmdStart[i]])) {
          perror("Unknown command, exec failed.\n");
          exit(2);
        }

      }
    }

    // close pipes in parent.
    for (int x = 0; x < cmdCount - 1; x++) {
      close(fd[x][0]);
      close(fd[x][1]);
    }

    // wait for all child process to complete
    int success = 1; // stays 1 only if every command succeeded
    for (int i = 0; i < cmdCount; i++) {
      int status;
      wait(&status);
      // if exit status non-zero, not succeeded
      if (WEXITSTATUS(status) != 0) {
        success = 0;
      }
    }

    // only successful commands are added to history
    if (success) {
      if (histCount == MAX_HIST) {
        // drop oldest then shift down
        for (int l = 1; l < MAX_HIST; l++) {
          strncpy(history[l-1], history[l], MAX_CHAR);
        }
        histCount--;
      }
      strncpy(history[histCount], historyLine, MAX_CHAR);
      histCount++;
    }
  }
  return 0;
}
